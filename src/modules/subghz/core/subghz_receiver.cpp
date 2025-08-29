#include "../../../../include/subghz/subghz_receiver.h"
#include "../../../../include/subghz/subghz_device.h"
#include "../../../../include/subghz/subghz_memory.h"
#include "../../../../include/subghz/subghz_protocol_registry.h"
#include "../../rf/rf_utils.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>

static const char *TAG = "SubGhzReceiver";

// Signal buffer for raw capture
#define SIGNAL_BUFFER_SIZE 1024

// Receiver structure
struct SubGhzReceiver {
    SubGhzDevice *device;
    SubGhzReceiverState state;
    SubGhzReceiverConfig config;

    // Protocol detection
    SubGhzProtocolRegistry *protocol_registry;

    // Signal processing
    uint32_t *signal_buffer;
    size_t buffer_size;
    size_t buffer_index;
    bool last_level;
    uint64_t last_time;

    // Callbacks
    SubGhzReceiverCallback callback;
    void *callback_context;
    SubGhzReceiverRawCallback raw_callback;
    void *raw_callback_context;

    // Task management
    TaskHandle_t task_handle;
    QueueHandle_t signal_queue;
    bool task_running;

    // Statistics
    uint32_t packets_received;
    uint32_t packets_decoded;
    uint32_t error_count;

    // Protocol filtering
    char *filter_protocol;
    bool filter_enabled;

    // RSSI monitoring
    int32_t current_rssi;
    uint64_t last_rssi_update;
};

// Signal data for queue
typedef struct {
    bool level;
    uint32_t duration;
    uint64_t timestamp;
} SignalData;

// Helper functions
static uint64_t get_current_time_us(void) { return esp_timer_get_time(); }

static void receiver_task(void *pvParameters);
static void receiver_process_signal(SubGhzReceiver *receiver, bool level, uint32_t duration);
static void receiver_feed_protocols(SubGhzReceiver *receiver, bool level, uint32_t duration);
static void receiver_update_rssi(SubGhzReceiver *receiver);
static void receiver_reset_buffer(SubGhzReceiver *receiver);

SubGhzReceiver *subghz_receiver_create(SubGhzDevice *device) {
    if (!device) {
        ESP_LOGE(TAG, "Invalid device");
        return NULL;
    }

    SubGhzReceiver *receiver =
        (SubGhzReceiver *)subghz_malloc(SUBGHZ_POOL_PROTOCOL, sizeof(SubGhzReceiver), 0);

    if (!receiver) {
        ESP_LOGE(TAG, "Failed to allocate receiver");
        return NULL;
    }

    memset(receiver, 0, sizeof(SubGhzReceiver));
    receiver->device = device;
    receiver->state = SUBGHZ_RECEIVER_STATE_IDLE;

    // Allocate signal buffer
    receiver->signal_buffer =
        (uint32_t *)subghz_malloc(SUBGHZ_POOL_SIGNAL, SIGNAL_BUFFER_SIZE * sizeof(uint32_t), 0);

    if (!receiver->signal_buffer) {
        ESP_LOGE(TAG, "Failed to allocate signal buffer");
        subghz_free(SUBGHZ_POOL_PROTOCOL, receiver);
        return NULL;
    }

    receiver->buffer_size = SIGNAL_BUFFER_SIZE;
    receiver->buffer_index = 0;

    // Initialize protocol registry (if not already done)
    if (!subghz_protocol_registry_init()) {
        ESP_LOGE(TAG, "Failed to initialize protocol registry");
        subghz_free(SUBGHZ_POOL_SIGNAL, receiver->signal_buffer);
        subghz_free(SUBGHZ_POOL_PROTOCOL, receiver);
        return NULL;
    }

    // Create signal queue
    receiver->signal_queue = xQueueCreate(100, sizeof(SignalData));
    if (!receiver->signal_queue) {
        ESP_LOGE(TAG, "Failed to create signal queue");
        subghz_free(SUBGHZ_POOL_SIGNAL, receiver->signal_buffer);
        subghz_free(SUBGHZ_POOL_PROTOCOL, receiver);
        return NULL;
    }

    // Create receiver task
    BaseType_t result = xTaskCreate(
        receiver_task, "SubGhzRx", 4096, receiver, configMAX_PRIORITIES - 2, &receiver->task_handle
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create receiver task");
        vQueueDelete(receiver->signal_queue);
        subghz_free(SUBGHZ_POOL_SIGNAL, receiver->signal_buffer);
        subghz_free(SUBGHZ_POOL_PROTOCOL, receiver);
        return NULL;
    }

    receiver->task_running = true;
    receiver->last_time = get_current_time_us();

    ESP_LOGI(TAG, "Receiver created successfully");
    return receiver;
}

void subghz_receiver_free(SubGhzReceiver *receiver) {
    if (!receiver) return;

    // Stop receiver first
    subghz_receiver_stop(receiver);

    // Stop task
    if (receiver->task_running) {
        receiver->task_running = false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Clean up resources
    if (receiver->signal_queue) { vQueueDelete(receiver->signal_queue); }


    if (receiver->signal_buffer) { subghz_free(SUBGHZ_POOL_SIGNAL, receiver->signal_buffer); }

    if (receiver->filter_protocol) { subghz_free(SUBGHZ_POOL_STRING, receiver->filter_protocol); }

    subghz_free(SUBGHZ_POOL_PROTOCOL, receiver);
    ESP_LOGI(TAG, "Receiver freed");
}

bool subghz_receiver_configure(SubGhzReceiver *receiver, const SubGhzReceiverConfig *config) {
    if (!receiver || !config) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    if (receiver->state == SUBGHZ_RECEIVER_STATE_RECEIVING) {
        ESP_LOGE(TAG, "Cannot configure while receiving");
        return false;
    }

    memcpy(&receiver->config, config, sizeof(SubGhzReceiverConfig));

    // Configure device
    if (!subghz_device_set_frequency(receiver->device, config->frequency)) {
        ESP_LOGE(TAG, "Failed to set frequency: %.2f MHz", config->frequency);
        return false;
    }

    if (!subghz_device_set_preset(receiver->device, config->preset)) {
        ESP_LOGE(TAG, "Failed to set preset");
        return false;
    }

    ESP_LOGI(
        TAG,
        "Receiver configured: %.2f MHz, RSSI threshold: %d dBm",
        config->frequency,
        config->rssi_threshold
    );
    return true;
}

void subghz_receiver_set_callback(SubGhzReceiver *receiver, SubGhzReceiverCallback callback, void *context) {
    if (!receiver) return;

    receiver->callback = callback;
    receiver->callback_context = context;
}

void subghz_receiver_set_raw_callback(
    SubGhzReceiver *receiver, SubGhzReceiverRawCallback callback, void *context
) {
    if (!receiver) return;

    receiver->raw_callback = callback;
    receiver->raw_callback_context = context;
}

bool subghz_receiver_start(SubGhzReceiver *receiver) {
    if (!receiver) {
        ESP_LOGE(TAG, "Invalid receiver");
        return false;
    }

    if (receiver->state == SUBGHZ_RECEIVER_STATE_RECEIVING) {
        ESP_LOGW(TAG, "Receiver already running");
        return true;
    }

    // Start RX mode
    if (!subghz_device_start_rx(receiver->device)) {
        ESP_LOGE(TAG, "Failed to start RX mode");
        return false;
    }

    receiver->state = SUBGHZ_RECEIVER_STATE_RECEIVING;
    receiver_reset_buffer(receiver);
    receiver->last_time = get_current_time_us();

    ESP_LOGI(TAG, "Receiver started");
    return true;
}

void subghz_receiver_stop(SubGhzReceiver *receiver) {
    if (!receiver) return;

    if (receiver->state == SUBGHZ_RECEIVER_STATE_RECEIVING) {
        subghz_device_stop_rx(receiver->device);
        receiver->state = SUBGHZ_RECEIVER_STATE_IDLE;
        ESP_LOGI(TAG, "Receiver stopped");
    }
}

bool subghz_receiver_is_running(SubGhzReceiver *receiver) {
    if (!receiver) return false;
    return receiver->state == SUBGHZ_RECEIVER_STATE_RECEIVING;
}

SubGhzReceiverState subghz_receiver_get_state(SubGhzReceiver *receiver) {
    if (!receiver) return SUBGHZ_RECEIVER_STATE_ERROR;
    return receiver->state;
}

int32_t subghz_receiver_get_rssi(SubGhzReceiver *receiver) {
    if (!receiver) return -100;

    receiver_update_rssi(receiver);
    return receiver->current_rssi;
}

void subghz_receiver_process(SubGhzReceiver *receiver) {
    if (!receiver || receiver->state != SUBGHZ_RECEIVER_STATE_RECEIVING) { return; }

    // This function can be called from main loop or ISR
    // For now, we rely on the internal task for processing
    receiver_update_rssi(receiver);
}

void subghz_receiver_feed_data(SubGhzReceiver *receiver, bool level, uint32_t duration) {
    if (!receiver || receiver->state != SUBGHZ_RECEIVER_STATE_RECEIVING) { return; }

    // Queue signal data for processing
    SignalData signal = {.level = level, .duration = duration, .timestamp = get_current_time_us()};

    if (xQueueSend(receiver->signal_queue, &signal, 0) != pdPASS) {
        // Queue full, drop signal
        receiver->error_count++;
    }
}

void subghz_receiver_reset(SubGhzReceiver *receiver) {
    if (!receiver) return;

    receiver_reset_buffer(receiver);

    // Reset protocol decoders
    if (receiver->protocol_registry) { subghz_protocol_registry_reset_all(); }

    ESP_LOGI(TAG, "Receiver reset");
}

bool subghz_receiver_get_stats(
    SubGhzReceiver *receiver, uint32_t *packets_received, uint32_t *packets_decoded, uint32_t *error_count
) {
    if (!receiver) return false;

    if (packets_received) *packets_received = receiver->packets_received;
    if (packets_decoded) *packets_decoded = receiver->packets_decoded;
    if (error_count) *error_count = receiver->error_count;

    return true;
}

bool subghz_receiver_set_protocol_filter(SubGhzReceiver *receiver, const char *protocol_name, bool enabled) {
    if (!receiver) return false;

    // Free existing filter
    if (receiver->filter_protocol) {
        subghz_free(SUBGHZ_POOL_STRING, receiver->filter_protocol);
        receiver->filter_protocol = NULL;
    }

    receiver->filter_enabled = enabled;

    if (enabled && protocol_name) {
        size_t len = strlen(protocol_name) + 1;
        receiver->filter_protocol = (char *)subghz_malloc(SUBGHZ_POOL_STRING, len, 0);
        if (receiver->filter_protocol) { strcpy(receiver->filter_protocol, protocol_name); }
    }

    return true;
}

// Task implementation
static void receiver_task(void *pvParameters) {
    SubGhzReceiver *receiver = (SubGhzReceiver *)pvParameters;
    SignalData signal;

    ESP_LOGI(TAG, "Receiver task started");

    while (receiver->task_running) {
        if (xQueueReceive(receiver->signal_queue, &signal, pdMS_TO_TICKS(100)) == pdPASS) {
            receiver_process_signal(receiver, signal.level, signal.duration);
            receiver->packets_received++;
        }

        // Update RSSI periodically
        receiver_update_rssi(receiver);
    }

    ESP_LOGI(TAG, "Receiver task finished");
    vTaskDelete(NULL);
}

static void receiver_process_signal(SubGhzReceiver *receiver, bool level, uint32_t duration) {
    // Call raw callback if enabled
    if (receiver->raw_callback) { receiver->raw_callback(receiver->raw_callback_context, level, duration); }

    // Store in buffer if raw capture enabled
    if (receiver->config.raw_capture_enabled && receiver->buffer_index < receiver->buffer_size) {
        receiver->signal_buffer[receiver->buffer_index++] = duration;
    }

    // Feed to protocol decoders
    receiver_feed_protocols(receiver, level, duration);

    receiver->last_level = level;
    receiver->last_time = get_current_time_us();
}

static void receiver_feed_protocols(SubGhzReceiver *receiver, bool level, uint32_t duration) {
    if (!receiver->protocol_registry) return;

    // Feed signal to all registered protocols
    subghz_protocol_registry_feed_decoders(level, duration);

    // For now, we'll implement a simple check
    // In a real implementation, this would check if any protocol decoded successfully
    SubGhzProtocolConfig *detected = nullptr; // Placeholder

    if (detected) {
        // Check protocol filter
        if (receiver->filter_enabled && receiver->filter_protocol) {
            if (strcmp(detected->name, receiver->filter_protocol) != 0) { return; }
        }

        receiver->packets_decoded++;

        // Call detection callback
        if (receiver->callback) { receiver->callback(receiver->callback_context, detected); }

        ESP_LOGI(TAG, "Protocol detected: %s", detected->name);
    }
}

static void receiver_update_rssi(SubGhzReceiver *receiver) {
    if (!receiver || !receiver->device) return;

    uint64_t current_time = get_current_time_us();
    if (current_time - receiver->last_rssi_update > 100000) { // 100ms
        receiver->current_rssi = subghz_device_get_rssi(receiver->device);
        receiver->last_rssi_update = current_time;
    }
}

static void receiver_reset_buffer(SubGhzReceiver *receiver) {
    if (!receiver) return;

    receiver->buffer_index = 0;
    receiver->last_level = false;

    // Clear signal queue
    SignalData signal;
    while (xQueueReceive(receiver->signal_queue, &signal, 0) == pdPASS) {
        // Just drain the queue
    }
}
