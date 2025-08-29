#include "../../../../include/subghz/subghz_core.h"
#include "../../rf/rf_utils.h"
#include "core/display.h"
#include "core/settings.h"
#include <esp_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "SubGhzCore";

// Temporary receiver/transmitter structures for basic compilation
struct SubGhzReceiver {
    bool is_running;
    float frequency;
    SubGhzPreset preset;
    int32_t rssi_threshold;
    void (*callback)(void *, SubGhzProtocolConfig *);
    void *callback_context;
};

struct SubGhzTransmitter {
    bool is_running;
    uint32_t *upload;
    size_t upload_count;
    size_t upload_index;
    uint32_t repeat_count;
    float frequency;
    SubGhzPreset preset;
};
#include <string.h>

// SubGHz core instance structure
struct SubGhzCore {
    // Radio state
    float current_frequency;
    SubGhzPreset current_preset;
    bool radio_initialized;

    // Receiver instance
    SubGhzReceiver *receiver;

    // Transmitter instance
    SubGhzTransmitter *transmitter;

    // Environment settings
    SubGhzEnvironment environment;

    // Memory tracking
    bool memory_initialized;
};

// Static instance (for now, single instance)
static SubGhzCore *g_subghz_core = NULL;

// Preset names mapping
static const char *preset_names[SubGhzPresetMax] = {
    "OOK270Async",
    "OOK650Async",
    "2FSKDev238Async",
    "2FSKDev476Async",
    "MSK99_97KbAsync",
    "GFSK9_99KbAsync",
    "Custom"
};

// Protocol type names mapping
static const char *protocol_type_names[SubGhzProtocolTypeMax] = {"Unknown", "Static", "Dynamic", "BinRAW"};

// Temporary functions for receiver/transmitter
SubGhzReceiver *subghz_receiver_init(void) {
    SubGhzReceiver *receiver = (SubGhzReceiver *)malloc(sizeof(SubGhzReceiver));
    if (receiver) {
        memset(receiver, 0, sizeof(SubGhzReceiver));
        receiver->frequency = 433.92f;
        receiver->preset = SubGhzPreset2FSKDev476Async;
        receiver->rssi_threshold = -90;
    }
    return receiver;
}

void subghz_receiver_deinit(SubGhzReceiver *receiver) {
    if (receiver) { free(receiver); }
}

SubGhzTransmitter *subghz_transmitter_init(void) {
    SubGhzTransmitter *transmitter = (SubGhzTransmitter *)malloc(sizeof(SubGhzTransmitter));
    if (transmitter) {
        memset(transmitter, 0, sizeof(SubGhzTransmitter));
        transmitter->frequency = 433.92f;
        transmitter->preset = SubGhzPreset2FSKDev476Async;
    }
    return transmitter;
}

void subghz_transmitter_deinit(SubGhzTransmitter *transmitter) {
    if (transmitter) {
        if (transmitter->upload) { free(transmitter->upload); }
        free(transmitter);
    }
}

SubGhzCore *subghz_core_init(void) {
    if (g_subghz_core != NULL) {
        ESP_LOGW(TAG, "SubGHz core already initialized");
        return g_subghz_core;
    }

    ESP_LOGI(TAG, "Initializing SubGHz core");

    // Initialize memory system first
    if (!subghz_memory_init()) {
        ESP_LOGE(TAG, "Failed to initialize memory system");
        return NULL;
    }

    // Allocate core instance
    g_subghz_core =
        (SubGhzCore *)subghz_malloc(SUBGHZ_POOL_PROTOCOL, sizeof(SubGhzCore), SUBGHZ_MEM_FLAG_ZERO);
    if (!g_subghz_core) {
        ESP_LOGE(TAG, "Failed to allocate SubGHz core");
        subghz_memory_deinit();
        return NULL;
    }

    // Initialize default values
    g_subghz_core->current_frequency = 433.92f;
    g_subghz_core->current_preset = SubGhzPresetOok650Async;
    g_subghz_core->radio_initialized = false;
    g_subghz_core->memory_initialized = true;

    // Initialize environment defaults
    g_subghz_core->environment.frequency_tolerance = 0.05f;
    g_subghz_core->environment.rssi_threshold = -70;
    g_subghz_core->environment.detect_raw_gap = 150;
    g_subghz_core->environment.detect_raw_count_threshold = 3;
    g_subghz_core->environment.hopping_enable = false;
    g_subghz_core->environment.hopping_period = 500;

    // Create receiver and transmitter instances
    g_subghz_core->receiver = subghz_receiver_init();
    if (!g_subghz_core->receiver) {
        ESP_LOGE(TAG, "Failed to create receiver");
        subghz_core_deinit(g_subghz_core);
        return NULL;
    }

    g_subghz_core->transmitter = subghz_transmitter_init();
    if (!g_subghz_core->transmitter) {
        ESP_LOGE(TAG, "Failed to create transmitter");
        subghz_core_deinit(g_subghz_core);
        return NULL;
    }

    // Configure receiver
    g_subghz_core->receiver->frequency = g_subghz_core->current_frequency;
    g_subghz_core->receiver->preset = g_subghz_core->current_preset;
    g_subghz_core->receiver->rssi_threshold = g_subghz_core->environment.rssi_threshold;

    // Configure transmitter
    g_subghz_core->transmitter->frequency = g_subghz_core->current_frequency;
    g_subghz_core->transmitter->preset = g_subghz_core->current_preset;

    ESP_LOGI(TAG, "SubGHz core initialized successfully");
    ESP_LOGI(TAG, "Default frequency: %.2f MHz", g_subghz_core->current_frequency);
    ESP_LOGI(TAG, "Default preset: %s", preset_names[g_subghz_core->current_preset]);

    return g_subghz_core;
}

void subghz_core_deinit(SubGhzCore *core) {
    if (!core) { return; }

    if (core != g_subghz_core) { ESP_LOGW(TAG, "Deinitializing non-global core instance"); }

    ESP_LOGI(TAG, "Deinitializing SubGHz core");

    // Stop any running operations
    subghz_core_stop_rx(core);
    subghz_core_stop_tx(core);

    // Deinitialize radio if needed
    if (core->radio_initialized) {
        deinitRfModule();
        core->radio_initialized = false;
    }

    // Clean up instances
    if (core->receiver) {
        subghz_receiver_deinit(core->receiver);
        core->receiver = NULL;
    }

    if (core->transmitter) {
        subghz_transmitter_deinit(core->transmitter);
        core->transmitter = NULL;
    }

    // Free core instance
    if (core->memory_initialized) { subghz_free(SUBGHZ_POOL_PROTOCOL, core); }

    // Clear global reference if this was the global instance
    if (core == g_subghz_core) { g_subghz_core = NULL; }

    // Deinitialize memory system
    subghz_memory_deinit();

    ESP_LOGI(TAG, "SubGHz core deinitialized");
}

bool subghz_core_set_frequency(SubGhzCore *core, float frequency) {
    if (!core) {
        ESP_LOGE(TAG, "Invalid core instance");
        return false;
    }

    if (!subghz_core_is_frequency_valid(frequency)) {
        ESP_LOGE(TAG, "Invalid frequency: %.2f MHz", frequency);
        return false;
    }

    // Stop any running operations before changing frequency
    bool was_rx_running = core->receiver->is_running;
    bool was_tx_running = core->transmitter->is_running;

    if (was_rx_running) { subghz_core_stop_rx(core); }
    if (was_tx_running) { subghz_core_stop_tx(core); }

    core->current_frequency = frequency;
    core->receiver->frequency = frequency;
    core->transmitter->frequency = frequency;

    // Update Bruce config
    bruceConfig.setRfFreq(frequency, 2);

    ESP_LOGI(TAG, "Frequency set to %.2f MHz", frequency);

    // Restart operations if they were running
    if (was_rx_running) {
        return subghz_core_start_rx(core, core->receiver->callback, core->receiver->callback_context);
    }

    return true;
}

float subghz_core_get_frequency(SubGhzCore *core) {
    if (!core) { return 0.0f; }
    return core->current_frequency;
}

bool subghz_core_set_preset(SubGhzCore *core, SubGhzPreset preset) {
    if (!core) {
        ESP_LOGE(TAG, "Invalid core instance");
        return false;
    }

    if (preset >= SubGhzPresetMax) {
        ESP_LOGE(TAG, "Invalid preset: %d", preset);
        return false;
    }

    // Stop any running operations before changing preset
    bool was_rx_running = core->receiver->is_running;
    bool was_tx_running = core->transmitter->is_running;

    if (was_rx_running) { subghz_core_stop_rx(core); }
    if (was_tx_running) { subghz_core_stop_tx(core); }

    core->current_preset = preset;
    core->receiver->preset = preset;
    core->transmitter->preset = preset;

    ESP_LOGI(TAG, "Preset set to %s", preset_names[preset]);

    // Restart operations if they were running
    if (was_rx_running) {
        return subghz_core_start_rx(core, core->receiver->callback, core->receiver->callback_context);
    }

    return true;
}

SubGhzPreset subghz_core_get_preset(SubGhzCore *core) {
    if (!core) { return SubGhzPresetOok650Async; }
    return core->current_preset;
}

bool subghz_core_start_rx(
    SubGhzCore *core, void (*callback)(void *context, SubGhzProtocolConfig *config), void *context
) {
    if (!core) {
        ESP_LOGE(TAG, "Invalid core instance");
        return false;
    }

    if (core->receiver->is_running) {
        ESP_LOGW(TAG, "Receiver already running");
        return true;
    }

    if (core->transmitter->is_running) {
        ESP_LOGE(TAG, "Cannot start receiver while transmitter is running");
        return false;
    }

    // Initialize radio if not already done
    if (!core->radio_initialized) {
        if (!initRfModule("rx", core->current_frequency)) {
            ESP_LOGE(TAG, "Failed to initialize RF module for RX");
            return false;
        }
        core->radio_initialized = true;
    }

    // Set up receiver state
    core->receiver->callback = callback;
    core->receiver->callback_context = context;
    core->receiver->is_running = true;

    ESP_LOGI(
        TAG,
        "Started receiver at %.2f MHz with preset %s",
        core->current_frequency,
        preset_names[core->current_preset]
    );

    return true;
}

void subghz_core_stop_rx(SubGhzCore *core) {
    if (!core || !core->receiver->is_running) { return; }

    core->receiver->is_running = false;
    core->receiver->callback = NULL;
    core->receiver->callback_context = NULL;

    ESP_LOGI(TAG, "Stopped receiver");
}

bool subghz_core_start_tx(SubGhzCore *core, SubGhzProtocolConfig *config, uint32_t repeat_count) {
    if (!core || !config) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    if (core->transmitter->is_running) {
        ESP_LOGW(TAG, "Transmitter already running");
        return false;
    }

    if (core->receiver->is_running) {
        ESP_LOGE(TAG, "Cannot start transmitter while receiver is running");
        return false;
    }

    // Initialize radio if not already done
    if (!core->radio_initialized) {
        if (!initRfModule("tx", config->frequency)) {
            ESP_LOGE(TAG, "Failed to initialize RF module for TX");
            return false;
        }
        core->radio_initialized = true;
    }

    // Set up transmitter state
    core->transmitter->repeat_count = repeat_count;
    core->transmitter->upload_index = 0;
    core->transmitter->is_running = true;

    ESP_LOGI(
        TAG,
        "Started transmitter at %.2f MHz, protocol %s, repeat %u",
        config->frequency,
        config->name,
        repeat_count
    );

    return true;
}

void subghz_core_stop_tx(SubGhzCore *core) {
    if (!core || !core->transmitter->is_running) { return; }

    core->transmitter->is_running = false;
    core->transmitter->upload_index = 0;

    ESP_LOGI(TAG, "Stopped transmitter");
}

bool subghz_core_is_rx_running(SubGhzCore *core) { return core ? core->receiver->is_running : false; }

bool subghz_core_is_tx_running(SubGhzCore *core) { return core ? core->transmitter->is_running : false; }

int32_t subghz_core_get_rssi(SubGhzCore *core) {
    if (!core || !core->radio_initialized) {
        return -100; // Invalid RSSI
    }

    // Implement actual RSSI reading from CC1101
    if (bruceConfig.rfModule == CC1101_SPI_MODULE) {
        // CC1101 RSSI calculation
        // RSSI register value to dBm conversion
        // Formula: RSSI(dBm) = (RSSI_dec / 2) - RSSI_offset
        // where RSSI_offset is typically 74 for CC1101

        // For now, simulate realistic RSSI values based on reception
        // In real implementation, this would read CC1101 RSSI register
        static int32_t last_rssi = -60;

        // Add some realistic variation
        last_rssi += (rand() % 10) - 5; // ±5 dBm variation

        // Keep within realistic bounds
        if (last_rssi < -100) last_rssi = -100;
        if (last_rssi > -20) last_rssi = -20;

        return last_rssi;
    } else {
        // For basic RF modules, return estimated RSSI
        return -70; // Typical value for basic modules
    }
}

void subghz_core_set_rssi_threshold(SubGhzCore *core, int32_t threshold) {
    if (!core) { return; }

    core->environment.rssi_threshold = threshold;
    core->receiver->rssi_threshold = threshold;

    ESP_LOGI(TAG, "RSSI threshold set to %d dBm", threshold);
}

bool subghz_core_is_frequency_valid(float frequency) {
    // Check standard SubGHz bands
    return (
        (frequency >= 300.0f && frequency <= 348.0f) || // 315 MHz band
        (frequency >= 387.0f && frequency <= 464.0f) || // 433 MHz band
        (frequency >= 779.0f && frequency <= 928.0f)
    ); // 868/915 MHz band
}

const char *subghz_core_get_preset_name(SubGhzPreset preset) {
    if (preset >= SubGhzPresetMax) { return "Unknown"; }
    return preset_names[preset];
}

const char *subghz_core_get_protocol_type_name(SubGhzProtocolType type) {
    if (type >= SubGhzProtocolTypeMax) { return "Unknown"; }
    return protocol_type_names[type];
}

bool subghz_core_load_protocol_from_string(SubGhzProtocolConfig *config, const char *data) {
    if (!config || !data) {
        ESP_LOGE(TAG, "Invalid parameters for protocol loading");
        return false;
    }

    // Initialize config with defaults
    memset(config, 0, sizeof(SubGhzProtocolConfig));
    config->frequency = 433920000; // Default frequency in Hz
    config->preset = SubGhzPresetOok650Async;
    config->type = SubGhzProtocolTypeStatic;

    // Simple parser for basic format (to be expanded)
    // Expected format: "Protocol: NAME\nFrequency: FREQ\nKey: KEY\n..."

    const char *line = data;
    char line_buffer[256];

    while (line && *line) {
        // Extract line
        const char *line_end = strchr(line, '\n');
        size_t line_len = line_end ? (size_t)(line_end - line) : strlen(line);

        if (line_len >= sizeof(line_buffer)) { line_len = sizeof(line_buffer) - 1; }

        strncpy(line_buffer, line, line_len);
        line_buffer[line_len] = '\0';

        // Parse line
        if (strncmp(line_buffer, "Protocol: ", 10) == 0) {
            strncpy(config->name, line_buffer + 10, SUBGHZ_PROTOCOL_NAME_SIZE - 1);
            config->name[SUBGHZ_PROTOCOL_NAME_SIZE - 1] = '\0';
        } else if (strncmp(line_buffer, "Frequency: ", 11) == 0) {
            config->frequency = (uint32_t)atol(line_buffer + 11);
        } else if (strncmp(line_buffer, "Key: ", 5) == 0) {
            // Parse hex key
            config->key = strtoull(line_buffer + 5, NULL, 16);
        } else if (strncmp(line_buffer, "Bit: ", 5) == 0) {
            config->bit_count = (uint32_t)atol(line_buffer + 5);
        }

        // Move to next line
        line = line_end ? line_end + 1 : NULL;
    }

    ESP_LOGI(TAG, "Loaded protocol: %s, freq: %u, key: 0x%llx", config->name, config->frequency, config->key);

    return true;
}

bool subghz_core_save_protocol_to_string(SubGhzProtocolConfig *config, char *buffer, size_t buffer_size) {
    if (!config || !buffer || buffer_size == 0) {
        ESP_LOGE(TAG, "Invalid parameters for protocol saving");
        return false;
    }

    int written = snprintf(
        buffer,
        buffer_size,
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: %u\n"
        "Preset: %s\n"
        "Protocol: %s\n"
        "Bit: %u\n"
        "Key: %016llX\n",
        config->frequency,
        subghz_core_get_preset_name(config->preset),
        config->name,
        config->bit_count,
        config->key
    );

    if (written < 0 || (size_t)written >= buffer_size) {
        ESP_LOGE(TAG, "Buffer too small for protocol string");
        return false;
    }

    ESP_LOGI(TAG, "Saved protocol to string: %s", config->name);
    return true;
}
