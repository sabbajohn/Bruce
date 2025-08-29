#include "subghz/subghz_types.h"
#include "subghz/subghz_memory.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "SubGhzTypes";

// Preset configurations based on CC1101 registers
static const SubGhzPresetConfig preset_configs[] = {
    // AM270 - Amplitude Modulation 270kHz (OOK)
    {.name = "AM270",
     .modulation = SubGhzModulationOOK,
     .bandwidth = 270000.0f,
     .data_rate = 4800.0f,
     .frequency_deviation = 0.0f,
     .preamble_size = 32,
     .sync_word = 0x2DD4,
     .manchester_enable = false},

    // AM650 - Amplitude Modulation 650kHz (OOK)
    {.name = "AM650",
     .modulation = SubGhzModulationOOK,
     .bandwidth = 650000.0f,
     .data_rate = 4800.0f,
     .frequency_deviation = 0.0f,
     .preamble_size = 32,
     .sync_word = 0x2DD4,
     .manchester_enable = false},

    // FM238 - Frequency Modulation 238kHz (2FSK)
    {.name = "FM238",
     .modulation = SubGhzModulation2FSK,
     .bandwidth = 270000.0f,
     .data_rate = 4800.0f,
     .frequency_deviation = 2380.0f,
     .preamble_size = 32,
     .sync_word = 0x2DD4,
     .manchester_enable = false},

    // FM476 - Frequency Modulation 476kHz (2FSK)
    {.name = "FM476",
     .modulation = SubGhzModulation2FSK,
     .bandwidth = 270000.0f,
     .data_rate = 4800.0f,
     .frequency_deviation = 4760.0f,
     .preamble_size = 32,
     .sync_word = 0x2DD4,
     .manchester_enable = false},

    // Custom preset for manual configuration
    {.name = "CUSTOM",
     .modulation = SubGhzModulation2FSK,
     .bandwidth = 0.0f,
     .data_rate = 0.0f,
     .frequency_deviation = 0.0f,
     .preamble_size = 32,
     .sync_word = 0x2DD4,
     .manchester_enable = false}
};

static const size_t preset_count = sizeof(preset_configs) / sizeof(preset_configs[0]);

bool subghz_get_preset_config(SubGhzPreset preset, SubGhzPresetConfig *config) {
    if (!config || preset >= SUBGHZ_PRESET_COUNT) {
        ESP_LOGE(TAG, "Invalid preset or config pointer");
        return false;
    }

    if (preset >= preset_count) {
        ESP_LOGE(TAG, "Preset index out of range: %d", preset);
        return false;
    }

    memcpy(config, &preset_configs[preset], sizeof(SubGhzPresetConfig));
    return true;
}

const char *subghz_get_preset_name(SubGhzPreset preset) {
    if (preset >= preset_count) { return "UNKNOWN"; }

    return preset_configs[preset].name;
}

SubGhzPreset subghz_get_preset_by_name(const char *name) {
    if (!name) { return SubGhzPresetCustom; }

    for (size_t i = 0; i < preset_count; i++) {
        if (strcmp(preset_configs[i].name, name) == 0) { return (SubGhzPreset)i; }
    }

    return SubGhzPresetCustom;
}

bool subghz_frequency_is_valid(float frequency) {
    // Check common SubGHz frequency ranges
    if (frequency >= 300.0f && frequency <= 348.0f) return true; // 315MHz band
    if (frequency >= 387.0f && frequency <= 464.0f) return true; // 433MHz band
    if (frequency >= 779.0f && frequency <= 928.0f) return true; // 868/915MHz band

    return false;
}

const char *subghz_modulation_to_string(SubGhzModulation modulation) {
    switch (modulation) {
        case SubGhzModulationOOK: return "OOK";
        case SubGhzModulation2FSK: return "2FSK";
        case SubGhzModulationGFSK: return "GFSK";
        case SubGhzModulationMSK: return "MSK";
        default: return "UNKNOWN";
    }
}

SubGhzModulation subghz_modulation_from_string(const char *str) {
    if (!str) return SubGhzModulation2FSK;

    if (strcmp(str, "OOK") == 0) return SubGhzModulationOOK;
    if (strcmp(str, "2FSK") == 0) return SubGhzModulation2FSK;
    if (strcmp(str, "GFSK") == 0) return SubGhzModulationGFSK;
    if (strcmp(str, "MSK") == 0) return SubGhzModulationMSK;

    return SubGhzModulation2FSK; // Default
}

bool subghz_protocol_config_create(
    SubGhzProtocolConfig *config, const char *name, SubGhzProtocolType type, const void *data,
    size_t data_size
) {
    if (!config || !name) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    memset(config, 0, sizeof(SubGhzProtocolConfig));

    // Copy name
    size_t name_len = strlen(name);
    if (name_len >= sizeof(config->name)) {
        ESP_LOGE(TAG, "Protocol name too long: %s", name);
        return false;
    }
    strcpy(config->name, name);

    config->type = type;
    config->frequency = 433.92f;          // Default frequency
    config->preset = SUBGHZ_PRESET_FM476; // Default preset

    // Copy data if provided
    if (data && data_size > 0) {
        if (data_size > sizeof(config->data)) {
            ESP_LOGE(TAG, "Protocol data too large: %d bytes", (int)data_size);
            return false;
        }

        memcpy(config->data, data, data_size);
        config->data_size = data_size;
    }

    return true;
}

void subghz_protocol_config_free(SubGhzProtocolConfig *config) {
    if (!config) return;

    // Clear sensitive data
    memset(config, 0, sizeof(SubGhzProtocolConfig));
}

bool subghz_protocol_config_copy(SubGhzProtocolConfig *dst, const SubGhzProtocolConfig *src) {
    if (!dst || !src) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    memcpy(dst, src, sizeof(SubGhzProtocolConfig));
    return true;
}

bool subghz_signal_create(SubGhzSignal *signal, size_t capacity) {
    if (!signal || capacity == 0) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    memset(signal, 0, sizeof(SubGhzSignal));

    // Allocate signal data
    signal->durations = (uint32_t *)malloc(capacity * sizeof(uint32_t));
    if (!signal->durations) {
        ESP_LOGE(TAG, "Failed to allocate signal data");
        return false;
    }

    signal->capacity = capacity;
    signal->count = 0;
    signal->level = false;
    signal->frequency = 433920000; // Default frequency
    signal->preset = SubGhzPreset2FSKDev238Async;

    return true;
}

void subghz_signal_free(SubGhzSignal *signal) {
    if (!signal) return;

    if (signal->durations) {
        free(signal->durations);
        signal->durations = NULL;
    }

    memset(signal, 0, sizeof(SubGhzSignal));
}

bool subghz_signal_add_duration(SubGhzSignal *signal, uint32_t duration) {
    if (!signal || !signal->durations) {
        ESP_LOGE(TAG, "Invalid signal");
        return false;
    }

    if (signal->count >= signal->capacity) {
        ESP_LOGE(TAG, "Signal buffer full");
        return false;
    }

    signal->durations[signal->count++] = duration;
    return true;
}

bool subghz_signal_add_edge(SubGhzSignal *signal, bool level, uint32_t duration) {
    // For now, just add duration - level tracking can be added later
    return subghz_signal_add_duration(signal, duration);
}

void subghz_signal_reset(SubGhzSignal *signal) {
    if (!signal) return;

    signal->count = 0;
    signal->level = false;
}

bool subghz_signal_copy(SubGhzSignal *dst, const SubGhzSignal *src) {
    if (!dst || !src || !src->durations) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    // Create destination if needed
    if (!dst->durations || dst->capacity < src->count) {
        if (dst->durations) {
            free(dst->durations);
            dst->durations = NULL;
        }

        dst->durations = (uint32_t *)malloc(src->capacity * sizeof(uint32_t));
        if (!dst->durations) {
            ESP_LOGE(TAG, "Failed to allocate destination signal data");
            return false;
        }
        dst->capacity = src->capacity;
    }

    // Copy data
    memcpy(dst->durations, src->durations, src->count * sizeof(uint32_t));
    dst->count = src->count;
    dst->level = src->level;
    dst->frequency = src->frequency;
    dst->preset = src->preset;

    return true;
}
