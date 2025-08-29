#include "subghz/subghz_capture.h"
#include "core/display.h"
#include "modules/rf/rf_utils.h"
#include "subghz/subghz_device.h"
#include "subghz/subghz_memory.h"
#include "subghz/subghz_receiver.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifdef USE_CC1101_VIA_SPI
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#endif

static const char *TAG = "SubGhzCapture";

// Global capture state
typedef struct {
    SubGhzDevice *device;
    SubGhzReceiver *receiver;
    SubGhzCaptureConfig config;
    SubGhzCaptureResult result;
    bool initialized;
    bool capturing;
    uint64_t capture_start_time;
    TaskHandle_t capture_task_handle;
} SubGhzCaptureState;

static SubGhzCaptureState g_capture_state = {0};

// Forward declarations
static void capture_task(void *pvParameters);
static void capture_signal_callback(void *context, SubGhzProtocolConfig *protocol);
static void capture_raw_callback(void *context, bool level, uint32_t duration);

bool subghz_capture_init(void) {
    if (g_capture_state.initialized) {
        ESP_LOGW(TAG, "Capture system already initialized");
        return true;
    }

    // Create device
    g_capture_state.device = subghz_device_create(SUBGHZ_DEVICE_TYPE_CC1101_SPI);
    if (!g_capture_state.device) {
        ESP_LOGE(TAG, "Failed to create SubGHz device");
        return false;
    }

    // Create receiver
    g_capture_state.receiver = subghz_receiver_create(g_capture_state.device);
    if (!g_capture_state.receiver) {
        ESP_LOGE(TAG, "Failed to create SubGHz receiver");
        subghz_device_free(g_capture_state.device);
        return false;
    }

    // Set callbacks
    subghz_receiver_set_callback(g_capture_state.receiver, capture_signal_callback, &g_capture_state);
    subghz_receiver_set_raw_callback(g_capture_state.receiver, capture_raw_callback, &g_capture_state);

    g_capture_state.initialized = true;
    g_capture_state.capturing = false;

    ESP_LOGI(TAG, "Capture system initialized successfully");
    return true;
}

void subghz_capture_deinit(void) {
    if (!g_capture_state.initialized) return;

    // Stop any ongoing capture
    subghz_capture_stop();

    // Free resources
    if (g_capture_state.receiver) {
        subghz_receiver_free(g_capture_state.receiver);
        g_capture_state.receiver = NULL;
    }

    if (g_capture_state.device) {
        subghz_device_free(g_capture_state.device);
        g_capture_state.device = NULL;
    }

    g_capture_state.initialized = false;
    ESP_LOGI(TAG, "Capture system deinitialized");
}

bool subghz_capture_start(const SubGhzCaptureConfig *config, SubGhzCaptureResult *result) {
    if (!g_capture_state.initialized) {
        ESP_LOGE(TAG, "Capture system not initialized");
        return false;
    }

    if (g_capture_state.capturing) {
        ESP_LOGW(TAG, "Capture already running");
        return false;
    }

    if (!config || !result) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    // Copy configuration
    g_capture_state.config = *config;
    memset(&g_capture_state.result, 0, sizeof(SubGhzCaptureResult));

    // Set frequency
    if (!subghz_device_set_frequency(g_capture_state.device, config->frequency)) {
        ESP_LOGE(TAG, "Failed to set frequency %.2f MHz", config->frequency);
        return false;
    }

    // Configure receiver
    SubGhzReceiverConfig rx_config = {
        .frequency = config->frequency,
        .preset = SubGhzPresetOok270Async,
        .rssi_threshold = config->rssi_threshold,
        .filter_enabled = false,
        .raw_capture_enabled = config->raw_capture_enabled,
        .timeout_ms = config->timeout_ms
    };
    subghz_receiver_configure(g_capture_state.receiver, &rx_config);

    g_capture_state.capturing = true;
    g_capture_state.capture_start_time = esp_timer_get_time() / 1000; // Convert to ms

    // Create capture task
    BaseType_t task_result = xTaskCreate(
        capture_task,
        "SubGhzCapture",
        4096,
        &g_capture_state,
        configMAX_PRIORITIES - 1,
        &g_capture_state.capture_task_handle
    );

    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create capture task");
        g_capture_state.capturing = false;
        return false;
    }

    ESP_LOGI(TAG, "Started capture on %.2f MHz", config->frequency);
    return true;
}

void subghz_capture_stop(void) {
    if (!g_capture_state.capturing) return;

    g_capture_state.capturing = false;

    // Stop receiver
    if (g_capture_state.receiver) { subghz_receiver_stop(g_capture_state.receiver); }

    // Wait for task to finish
    if (g_capture_state.capture_task_handle) {
        vTaskDelete(g_capture_state.capture_task_handle);
        g_capture_state.capture_task_handle = NULL;
    }

    ESP_LOGI(TAG, "Capture stopped");
}

bool subghz_capture_is_running(void) { return g_capture_state.capturing; }

bool subghz_capture_get_stats(SubGhzCaptureResult *result) {
    if (!result || !g_capture_state.initialized) return false;

    if (g_capture_state.capturing) {
        // Update current duration
        uint64_t current_time = esp_timer_get_time() / 1000;
        g_capture_state.result.capture_duration_ms = current_time - g_capture_state.capture_start_time;
    }

    *result = g_capture_state.result;
    return true;
}

float subghz_capture_frequency_scan(
    float start_freq, float end_freq, float step_freq, uint32_t scan_time_ms
) {
    if (!g_capture_state.initialized) {
        ESP_LOGE(TAG, "Capture system not initialized");
        return 0.0f;
    }

#ifdef USE_CC1101_VIA_SPI
    // Evita concorrência SPI: para receiver antes do scan
    bool receiver_was_running = false;
    if (g_capture_state.receiver && subghz_receiver_is_running(g_capture_state.receiver)) {
        subghz_receiver_stop(g_capture_state.receiver);
        receiver_was_running = true;
        vTaskDelay(pdMS_TO_TICKS(20)); // Aguarda SPI liberar
    }
    float best_freq = 0.0f;
    int best_rssi = -100;
    float current_freq = start_freq;

    ESP_LOGI(TAG, "Starting frequency scan from %.2f to %.2f MHz", start_freq, end_freq);

    while (current_freq <= end_freq) {
        // Set frequency
        if (subghz_device_set_frequency(g_capture_state.device, current_freq)) {

            // Give CC1101 time to settle
            vTaskDelay(pdMS_TO_TICKS(10));

            // Sample RSSI multiple times
            int rssi_sum = 0;
            int rssi_samples = scan_time_ms / 10; // Sample every 10ms
            if (rssi_samples < 1) rssi_samples = 1;

            for (int i = 0; i < rssi_samples; i++) {
                int rssi = ELECHOUSE_cc1101.getRssi();
                rssi_sum += rssi;
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            int avg_rssi = rssi_sum / rssi_samples;

            ESP_LOGD(TAG, "Freq %.2f MHz: RSSI %d dBm", current_freq, avg_rssi);

            if (avg_rssi > best_rssi) {
                best_rssi = avg_rssi;
                best_freq = current_freq;
            }
        }

        current_freq += step_freq;
    }

    if (best_freq > 0) {
        ESP_LOGI(TAG, "Best frequency found: %.2f MHz (RSSI: %d dBm)", best_freq, best_rssi);
    } else {
        ESP_LOGW(TAG, "No strong signals found in scan range");
    }

    // Reativa receiver se estava rodando antes do scan
    if (receiver_was_running && g_capture_state.receiver) { subghz_receiver_start(g_capture_state.receiver); }
    return best_freq;
#else
    ESP_LOGW(TAG, "CC1101 not available for frequency scan");
    return 0.0f;
#endif
}

// Private function implementations
static void capture_task(void *pvParameters) {
    SubGhzCaptureState *state = (SubGhzCaptureState *)pvParameters;

    ESP_LOGI(TAG, "Capture task started");

    // Start receiver
    if (!subghz_receiver_start(state->receiver)) {
        ESP_LOGE(TAG, "Failed to start receiver");
        state->capturing = false;
        vTaskDelete(NULL);
        return;
    }

    uint64_t last_rssi_update = 0;
    uint32_t rssi_samples = 0;
    float rssi_sum = 0;

    while (state->capturing) {
        uint64_t current_time = esp_timer_get_time() / 1000; // Convert to ms

        // Check timeout
        if (state->config.timeout_ms > 0) {
            if ((current_time - state->capture_start_time) >= state->config.timeout_ms) {
                ESP_LOGI(TAG, "Capture timeout reached");
                state->result.timeout_reached = true;
                break;
            }
        }

        // Check maximum signals
        if (state->config.max_signals > 0) {
            if (state->result.signals_captured >= state->config.max_signals) {
                ESP_LOGI(TAG, "Maximum signals captured");
                break;
            }
        }

        // Update RSSI periodically
        if ((current_time - last_rssi_update) >= 100) { // Every 100ms
            int32_t rssi = subghz_device_get_rssi(state->device);
            if (rssi > -100) { // Valid RSSI
                rssi_sum += rssi;
                rssi_samples++;
                state->result.average_rssi = rssi_sum / rssi_samples;
            }
            last_rssi_update = current_time;
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay
    }

    // Final duration update
    state->result.capture_duration_ms = esp_timer_get_time() / 1000 - state->capture_start_time;

    // Stop receiver
    subghz_receiver_stop(state->receiver);

    state->capturing = false;
    ESP_LOGI(
        TAG,
        "Capture task finished - captured %d signals in %d ms",
        state->result.signals_captured,
        state->result.capture_duration_ms
    );

    vTaskDelete(NULL);
}

static void capture_signal_callback(void *context, SubGhzProtocolConfig *protocol) {
    SubGhzCaptureState *state = (SubGhzCaptureState *)context;
    if (!state || !state->capturing) return;

    state->result.signals_captured++;
    state->result.protocols_detected++;

    ESP_LOGI(
        TAG,
        "Protocol detected: %s (Signal #%d)",
        protocol ? protocol->name : "Unknown",
        state->result.signals_captured
    );
}

static void capture_raw_callback(void *context, bool level, uint32_t duration) {
    SubGhzCaptureState *state = (SubGhzCaptureState *)context;
    if (!state || !state->capturing) return;

    // Count raw signal transitions
    static uint32_t transition_count = 0;
    transition_count++;

    // Consider every 100 transitions as one "signal" for counting purposes
    if (transition_count % 100 == 0) { state->result.signals_captured++; }

    ESP_LOGV(TAG, "Raw signal: level=%d, duration=%d us", level, duration);
}
