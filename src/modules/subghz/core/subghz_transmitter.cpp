#include "../../../../include/subghz/subghz_transmitter.h"
#include "../../../../include/subghz/subghz_device.h"
#include "../../../../include/subghz/subghz_memory.h"
#include "../../../../include/subghz/subghz_protocol_registry.h"
#include "../../rf/rf_utils.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>

static const char *TAG = "SubGhzTransmitter";

// Transmitter structure
struct SubGhzTransmitter {
    SubGhzDevice *device;
    SubGhzTransmitterState state;
    SubGhzTransmitterConfig config;

    // Signal data
    uint32_t *data;
    size_t data_size;
    size_t data_capacity;
    size_t current_index;

    // Timing control
    uint64_t start_time;
    uint64_t next_toggle_time;
    bool current_level;

    // Callback
    SubGhzTransmitterCallback callback;
    void *callback_context;

    // Task management
    TaskHandle_t task_handle;
    QueueHandle_t command_queue;
    bool task_running;

    // Statistics
    uint32_t packets_sent;
    uint32_t repeat_count;
    uint32_t error_count;
};

// Internal commands
typedef enum { TRANSMITTER_CMD_SEND, TRANSMITTER_CMD_STOP, TRANSMITTER_CMD_SHUTDOWN } TransmitterCommand;

typedef struct {
    TransmitterCommand cmd;
    union {
        struct {
            uint32_t *data;
            size_t size;
            uint32_t repeat;
        } send;
    } params;
} TransmitterCommandData;

// Helper functions
static uint64_t get_current_time_us(void) { return esp_timer_get_time(); }

static void transmitter_task(void *pvParameters);
static bool transmitter_prepare_data(SubGhzTransmitter *transmitter, const uint32_t *data, size_t size);
static void transmitter_execute_transmission(SubGhzTransmitter *transmitter);
static void transmitter_notify_callback(SubGhzTransmitter *transmitter, SubGhzTransmitterState event);

SubGhzTransmitter *subghz_transmitter_create(SubGhzDevice *device) {
    if (!device) {
        ESP_LOGE(TAG, "Invalid device");
        return NULL;
    }

    SubGhzTransmitter *transmitter =
        (SubGhzTransmitter *)subghz_malloc(SUBGHZ_POOL_PROTOCOL, sizeof(SubGhzTransmitter), 0);

    if (!transmitter) {
        ESP_LOGE(TAG, "Failed to allocate transmitter");
        return NULL;
    }

    memset(transmitter, 0, sizeof(SubGhzTransmitter));
    transmitter->device = device;
    transmitter->state = SUBGHZ_TRANSMITTER_STATE_IDLE;

    // Create command queue
    transmitter->command_queue = xQueueCreate(5, sizeof(TransmitterCommandData));
    if (!transmitter->command_queue) {
        ESP_LOGE(TAG, "Failed to create command queue");
        subghz_free(SUBGHZ_POOL_PROTOCOL, transmitter);
        return NULL;
    }

    // Create transmitter task
    BaseType_t result = xTaskCreate(
        transmitter_task, "SubGhzTx", 4096, transmitter, configMAX_PRIORITIES - 1, &transmitter->task_handle
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create transmitter task");
        vQueueDelete(transmitter->command_queue);
        subghz_free(SUBGHZ_POOL_PROTOCOL, transmitter);
        return NULL;
    }

    transmitter->task_running = true;

    ESP_LOGI(TAG, "Transmitter created successfully");
    return transmitter;
}

void subghz_transmitter_free(SubGhzTransmitter *transmitter) {
    if (!transmitter) return;

    // Stop task
    if (transmitter->task_running) {
        TransmitterCommandData cmd = {};
        cmd.cmd = TRANSMITTER_CMD_SHUTDOWN;
        xQueueSend(transmitter->command_queue, &cmd, portMAX_DELAY);

        // Wait for task to finish
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Clean up resources
    if (transmitter->command_queue) { vQueueDelete(transmitter->command_queue); }

    if (transmitter->data) { subghz_free(SUBGHZ_POOL_SIGNAL, transmitter->data); }

    subghz_free(SUBGHZ_POOL_PROTOCOL, transmitter);
    ESP_LOGI(TAG, "Transmitter freed");
}

bool subghz_transmitter_configure(SubGhzTransmitter *transmitter, const SubGhzTransmitterConfig *config) {
    if (!transmitter || !config) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    if (transmitter->state == SUBGHZ_TRANSMITTER_STATE_TRANSMITTING) {
        ESP_LOGE(TAG, "Cannot configure while transmitting");
        return false;
    }

    memcpy(&transmitter->config, config, sizeof(SubGhzTransmitterConfig));

    // Configure device
    if (!subghz_device_set_frequency(transmitter->device, config->frequency)) {
        ESP_LOGE(TAG, "Failed to set frequency: %.2f MHz", config->frequency);
        return false;
    }

    if (!subghz_device_set_preset(transmitter->device, config->preset)) {
        ESP_LOGE(TAG, "Failed to set preset");
        return false;
    }

    if (!subghz_device_set_power(transmitter->device, config->power)) {
        ESP_LOGE(TAG, "Failed to set TX power: %d dBm", config->power);
        return false;
    }

    ESP_LOGI(TAG, "Transmitter configured: %.2f MHz, %d dBm", config->frequency, config->power);
    return true;
}

void subghz_transmitter_set_callback(
    SubGhzTransmitter *transmitter, SubGhzTransmitterCallback callback, void *context
) {
    if (!transmitter) return;

    transmitter->callback = callback;
    transmitter->callback_context = context;
}

bool subghz_transmitter_send_async(
    SubGhzTransmitter *transmitter, const uint32_t *data, size_t size, uint32_t repeat
) {
    if (!transmitter || !data || size == 0) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    if (transmitter->state == SUBGHZ_TRANSMITTER_STATE_TRANSMITTING) {
        ESP_LOGE(TAG, "Transmitter is busy");
        return false;
    }

    // Allocate data copy
    uint32_t *data_copy = (uint32_t *)subghz_malloc(SUBGHZ_POOL_SIGNAL, size * sizeof(uint32_t), 0);
    if (!data_copy) {
        ESP_LOGE(TAG, "Failed to allocate signal data");
        return false;
    }

    memcpy(data_copy, data, size * sizeof(uint32_t));

    // Send command to task
    TransmitterCommandData cmd = {};
    cmd.cmd = TRANSMITTER_CMD_SEND;
    cmd.params.send.data = data_copy;
    cmd.params.send.size = size;
    cmd.params.send.repeat = repeat;

    if (xQueueSend(transmitter->command_queue, &cmd, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to queue transmission command");
        subghz_free(SUBGHZ_POOL_SIGNAL, data_copy);
        return false;
    }

    return true;
}

void subghz_transmitter_stop(SubGhzTransmitter *transmitter) {
    if (!transmitter) return;

    TransmitterCommandData cmd = {};
    cmd.cmd = TRANSMITTER_CMD_STOP;
    xQueueSend(transmitter->command_queue, &cmd, 0);
}

bool subghz_transmitter_is_running(SubGhzTransmitter *transmitter) {
    if (!transmitter) return false;
    return transmitter->state == SUBGHZ_TRANSMITTER_STATE_TRANSMITTING;
}

SubGhzTransmitterState subghz_transmitter_get_state(SubGhzTransmitter *transmitter) {
    if (!transmitter) return SUBGHZ_TRANSMITTER_STATE_ERROR;
    return transmitter->state;
}

bool subghz_transmitter_get_stats(
    SubGhzTransmitter *transmitter, uint32_t *packets_sent, uint32_t *repeat_count, uint32_t *error_count
) {
    if (!transmitter) return false;

    if (packets_sent) *packets_sent = transmitter->packets_sent;
    if (repeat_count) *repeat_count = transmitter->repeat_count;
    if (error_count) *error_count = transmitter->error_count;

    return true;
}

void subghz_transmitter_reset_stats(SubGhzTransmitter *transmitter) {
    if (!transmitter) return;

    transmitter->packets_sent = 0;
    transmitter->repeat_count = 0;
    transmitter->error_count = 0;
}

// Task implementation
static void transmitter_task(void *pvParameters) {
    SubGhzTransmitter *transmitter = (SubGhzTransmitter *)pvParameters;
    TransmitterCommandData cmd;

    ESP_LOGI(TAG, "Transmitter task started");

    while (transmitter->task_running) {
        if (xQueueReceive(transmitter->command_queue, &cmd, pdMS_TO_TICKS(100)) == pdPASS) {
            switch (cmd.cmd) {
                case TRANSMITTER_CMD_SEND:
                    ESP_LOGI(
                        TAG,
                        "Starting transmission of %d samples, %d repeats",
                        (int)cmd.params.send.size,
                        (int)cmd.params.send.repeat
                    );

                    if (transmitter_prepare_data(transmitter, cmd.params.send.data, cmd.params.send.size)) {
                        transmitter->config.repeat_count = cmd.params.send.repeat;
                        transmitter->state = SUBGHZ_TRANSMITTER_STATE_TRANSMITTING;
                        transmitter_notify_callback(transmitter, SUBGHZ_TRANSMITTER_STATE_TRANSMITTING);
                        transmitter_execute_transmission(transmitter);
                        transmitter->state = SUBGHZ_TRANSMITTER_STATE_IDLE;
                        transmitter_notify_callback(transmitter, SUBGHZ_TRANSMITTER_STATE_IDLE);
                    } else {
                        transmitter->error_count++;
                        transmitter_notify_callback(transmitter, SUBGHZ_TRANSMITTER_STATE_ERROR);
                    }

                    // Free command data
                    subghz_free(SUBGHZ_POOL_SIGNAL, cmd.params.send.data);
                    break;

                case TRANSMITTER_CMD_STOP:
                    ESP_LOGI(TAG, "Stopping transmission");
                    transmitter->state = SUBGHZ_TRANSMITTER_STATE_IDLE;
                    transmitter_notify_callback(transmitter, SUBGHZ_TRANSMITTER_STATE_IDLE);
                    break;

                case TRANSMITTER_CMD_SHUTDOWN:
                    ESP_LOGI(TAG, "Shutting down transmitter task");
                    transmitter->task_running = false;
                    break;
            }
        }
    }

    ESP_LOGI(TAG, "Transmitter task finished");
    vTaskDelete(NULL);
}

static bool transmitter_prepare_data(SubGhzTransmitter *transmitter, const uint32_t *data, size_t size) {
    // Free previous data
    if (transmitter->data) {
        subghz_free(SUBGHZ_POOL_SIGNAL, transmitter->data);
        transmitter->data = NULL;
    }

    // Allocate new data
    transmitter->data = (uint32_t *)subghz_malloc(SUBGHZ_POOL_SIGNAL, size * sizeof(uint32_t), 0);
    if (!transmitter->data) {
        ESP_LOGE(TAG, "Failed to allocate transmission data");
        return false;
    }

    memcpy(transmitter->data, data, size * sizeof(uint32_t));
    transmitter->data_size = size;
    transmitter->data_capacity = size;
    transmitter->current_index = 0;

    return true;
}

static void transmitter_execute_transmission(SubGhzTransmitter *transmitter) {
    if (!transmitter || !transmitter->data || transmitter->data_size == 0) {
        ESP_LOGE(TAG, "Invalid transmission data");
        return;
    }

    // Enter TX mode
    if (!subghz_device_start_tx(transmitter->device, transmitter->data, transmitter->data_size)) {
        ESP_LOGE(TAG, "Failed to start TX mode");
        transmitter->error_count++;
        return;
    }

    uint32_t repeats = (transmitter->config.repeat_count == 0) ? 1 : transmitter->config.repeat_count;

    for (uint32_t repeat = 0; repeat < repeats && transmitter->state == SUBGHZ_TRANSMITTER_STATE_TRANSMITTING;
         repeat++) {
        transmitter->current_index = 0;
        transmitter->current_level = true; // Start with high level
        transmitter->start_time = get_current_time_us();
        transmitter->next_toggle_time = transmitter->start_time;

        // Transmit all samples
        while (transmitter->current_index < transmitter->data_size &&
               transmitter->state == SUBGHZ_TRANSMITTER_STATE_TRANSMITTING) {

            uint32_t duration = transmitter->data[transmitter->current_index];
            transmitter->next_toggle_time += duration;

            // Set pin level based on timing
            subghz_device_write_pin(transmitter->device, transmitter->current_level);

            // Wait for exact timing
            uint64_t current_time = get_current_time_us();
            if (transmitter->next_toggle_time > current_time) {
                uint32_t delay_us = (uint32_t)(transmitter->next_toggle_time - current_time);
                if (delay_us > 1000) {
                    vTaskDelay(pdMS_TO_TICKS(delay_us / 1000));
                } else {
                    esp_rom_delay_us(delay_us);
                }
            }

            transmitter->current_level = !transmitter->current_level;
            transmitter->current_index++;
        }

        transmitter->repeat_count++;

        // Add gap between repeats
        if (repeat < repeats - 1 && transmitter->config.repeat_delay_ms > 0) {
            subghz_device_write_pin(transmitter->device, false);
            vTaskDelay(pdMS_TO_TICKS(transmitter->config.repeat_delay_ms));
        }
    }

    // Ensure pin is low at end
    subghz_device_write_pin(transmitter->device, false);

    // Exit TX mode
    subghz_device_stop_tx(transmitter->device);

    transmitter->packets_sent++;
    ESP_LOGI(TAG, "Transmission completed: %d repeats", transmitter->repeat_count);
}

static void transmitter_notify_callback(SubGhzTransmitter *transmitter, SubGhzTransmitterState event) {
    if (transmitter && transmitter->callback) {
        transmitter->callback(transmitter->callback_context, event, 0); // progress = 0 as default
    }
}
