#include "subghz/subghz_protocol_registry.h"
#include "subghz/subghz_protocol_came.h"
#include "subghz/subghz_protocol_keeloq.h"
#include "subghz/subghz_protocol_princeton.h"
#include "subghz/subghz_protocol_somfy.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "SubGhzRegistry";

// Global protocol registry
static SubGhzProtocolRegistry g_registry = {0};

// Built-in protocol definitions
extern const SubGhzProtocolDefinition princeton_protocol_definition;

bool subghz_protocol_registry_init(void) {
    if (g_registry.initialized) {
        ESP_LOGW(TAG, "Protocol registry already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing protocol registry");

    memset(&g_registry, 0, sizeof(SubGhzProtocolRegistry));
    g_registry.initialized = true;

    // Register built-in protocols
    size_t builtin_count = subghz_protocol_registry_register_builtin();

    ESP_LOGI(TAG, "Protocol registry initialized with %zu built-in protocols", builtin_count);
    return true;
}

void subghz_protocol_registry_deinit(void) {
    if (!g_registry.initialized) { return; }

    ESP_LOGI(TAG, "Deinitializing protocol registry");

    // Free all protocol decoders and encoders
    for (size_t i = 0; i < g_registry.count; i++) {
        SubGhzProtocolRegistryEntry *entry = &g_registry.entries[i];

        if (entry->decoder && entry->definition.interface.decoder_free) {
            entry->definition.interface.decoder_free(entry->decoder);
            entry->decoder = NULL;
        }

        if (entry->encoder && entry->definition.interface.encoder_free) {
            entry->definition.interface.encoder_free(entry->encoder);
            entry->encoder = NULL;
        }
    }

    memset(&g_registry, 0, sizeof(SubGhzProtocolRegistry));
    ESP_LOGI(TAG, "Protocol registry deinitialized");
}

bool subghz_protocol_registry_register(const SubGhzProtocolDefinition *definition) {
    if (!definition || !definition->name) {
        ESP_LOGE(TAG, "Invalid protocol definition");
        return false;
    }

    if (!g_registry.initialized) {
        ESP_LOGE(TAG, "Registry not initialized");
        return false;
    }

    if (g_registry.count >= SUBGHZ_PROTOCOL_MAX_COUNT) {
        ESP_LOGE(TAG, "Protocol registry full");
        return false;
    }

    // Check if protocol already exists
    if (subghz_protocol_registry_get_by_name(definition->name) != NULL) {
        ESP_LOGW(TAG, "Protocol %s already registered", definition->name);
        return false;
    }

    // Add protocol to registry
    SubGhzProtocolRegistryEntry *entry = &g_registry.entries[g_registry.count];
    entry->definition = *definition;
    entry->decoder = NULL;
    entry->encoder = NULL;
    entry->decode_count = 0;
    entry->encode_count = 0;
    entry->error_count = 0;

    // Create decoder instance if interface is available
    if (entry->definition.interface.decoder_alloc) {
        entry->decoder = entry->definition.interface.decoder_alloc();
        if (!entry->decoder) {
            ESP_LOGE(TAG, "Failed to create decoder for protocol %s", definition->name);
            return false;
        }
    }

    g_registry.count++;

    ESP_LOGI(
        TAG,
        "Registered protocol: %s (type: %d, freq: %u)",
        definition->name,
        definition->type,
        definition->frequency
    );

    return true;
}

bool subghz_protocol_registry_unregister(const char *name) {
    if (!name || !g_registry.initialized) { return false; }

    // Find protocol
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.entries[i].definition.name, name) == 0) {
            SubGhzProtocolRegistryEntry *entry = &g_registry.entries[i];

            // Free decoder and encoder
            if (entry->decoder && entry->definition.interface.decoder_free) {
                entry->definition.interface.decoder_free(entry->decoder);
            }
            if (entry->encoder && entry->definition.interface.encoder_free) {
                entry->definition.interface.encoder_free(entry->encoder);
            }

            // Shift remaining entries
            for (size_t j = i; j < g_registry.count - 1; j++) {
                g_registry.entries[j] = g_registry.entries[j + 1];
            }

            g_registry.count--;
            ESP_LOGI(TAG, "Unregistered protocol: %s", name);
            return true;
        }
    }

    ESP_LOGW(TAG, "Protocol %s not found for unregistration", name);
    return false;
}

SubGhzProtocolRegistryEntry *subghz_protocol_registry_get_by_name(const char *name) {
    if (!name || !g_registry.initialized) { return NULL; }

    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.entries[i].definition.name, name) == 0) { return &g_registry.entries[i]; }
    }

    return NULL;
}

SubGhzProtocolRegistryEntry *subghz_protocol_registry_get_by_index(size_t index) {
    if (!g_registry.initialized || index >= g_registry.count) { return NULL; }

    return &g_registry.entries[index];
}

size_t subghz_protocol_registry_get_count(void) {
    if (!g_registry.initialized) { return 0; }

    return g_registry.count;
}

bool subghz_protocol_registry_set_enabled(const char *name, bool enabled) {
    SubGhzProtocolRegistryEntry *entry = subghz_protocol_registry_get_by_name(name);
    if (!entry) { return false; }

    entry->definition.enabled = enabled;
    ESP_LOGI(TAG, "Protocol %s %s", name, enabled ? "enabled" : "disabled");
    return true;
}

bool subghz_protocol_registry_is_enabled(const char *name) {
    SubGhzProtocolRegistryEntry *entry = subghz_protocol_registry_get_by_name(name);
    if (!entry) { return false; }

    return entry->definition.enabled;
}

void subghz_protocol_registry_set_decoder_callback(SubGhzProtocolDecoderCallback callback, void *context) {
    if (!g_registry.initialized) { return; }

    g_registry.decoder_callback = callback;
    g_registry.decoder_context = context;
    ESP_LOGI(TAG, "Decoder callback set");
}

void subghz_protocol_registry_reset_decoders(void) {
    if (!g_registry.initialized) { return; }

    for (size_t i = 0; i < g_registry.count; i++) {
        SubGhzProtocolRegistryEntry *entry = &g_registry.entries[i];
        if (entry->decoder && entry->definition.interface.decoder_reset) {
            entry->definition.interface.decoder_reset(entry->decoder);
        }
    }

    ESP_LOGD(TAG, "All decoders reset");
}

void subghz_protocol_registry_feed_decoders(bool level, uint32_t duration) {
    if (!g_registry.initialized) { return; }

    for (size_t i = 0; i < g_registry.count; i++) {
        SubGhzProtocolRegistryEntry *entry = &g_registry.entries[i];

        if (!entry->definition.enabled || !entry->decoder || !entry->definition.interface.decoder_feed) {
            continue;
        }

        // Feed data to decoder
        entry->definition.interface.decoder_feed(entry->decoder, level, duration);

        // Check if decoder has data
        if (entry->definition.interface.decoder_get_data) {
            SubGhzProtocolConfig config;
            if (entry->definition.interface.decoder_get_data(entry->decoder, &config)) {
                entry->decode_count++;

                // Call callback if set
                if (g_registry.decoder_callback) {
                    g_registry.decoder_callback(g_registry.decoder_context, &config);
                }

                ESP_LOGI(
                    TAG,
                    "Protocol %s decoded data: key=0x%08X, serial=0x%05X",
                    entry->definition.name,
                    (uint32_t)config.key,
                    config.serial
                );
            }
        }
    }
}

SubGhzProtocolEncoder *
subghz_protocol_registry_create_encoder(const char *name, SubGhzProtocolConfig *config) {
    SubGhzProtocolRegistryEntry *entry = subghz_protocol_registry_get_by_name(name);
    if (!entry || !config) { return NULL; }

    if (!entry->definition.interface.encoder_alloc || !entry->definition.interface.encoder_set_data) {
        ESP_LOGE(TAG, "Protocol %s does not support encoding", name);
        return NULL;
    }

    SubGhzProtocolEncoder *encoder = entry->definition.interface.encoder_alloc();
    if (!encoder) {
        ESP_LOGE(TAG, "Failed to create encoder for protocol %s", name);
        entry->error_count++;
        return NULL;
    }

    if (!entry->definition.interface.encoder_set_data(encoder, config)) {
        ESP_LOGE(TAG, "Failed to set encoder data for protocol %s", name);
        entry->definition.interface.encoder_free(encoder);
        entry->error_count++;
        return NULL;
    }

    entry->encode_count++;
    ESP_LOGI(TAG, "Created encoder for protocol %s", name);
    return encoder;
}

void subghz_protocol_registry_free_encoder(SubGhzProtocolEncoder *encoder, const char *name) {
    if (!encoder) { return; }

    SubGhzProtocolRegistryEntry *entry = subghz_protocol_registry_get_by_name(name);
    if (entry && entry->definition.interface.encoder_free) {
        entry->definition.interface.encoder_free(encoder);
        ESP_LOGD(TAG, "Freed encoder for protocol %s", name);
    }
}

uint32_t *subghz_protocol_registry_get_encoder_upload(SubGhzProtocolEncoder *encoder, size_t *upload_count) {
    // This is a generic function - specific implementation depends on the encoder type
    // For now, we'll need the protocol name to find the right interface function
    // In a real implementation, the encoder would have a vtable or type field
    return NULL;
}

bool subghz_protocol_registry_get_stats(
    const char *name, uint32_t *decode_count, uint32_t *encode_count, uint32_t *error_count
) {
    SubGhzProtocolRegistryEntry *entry = subghz_protocol_registry_get_by_name(name);
    if (!entry) { return false; }

    if (decode_count) *decode_count = entry->decode_count;
    if (encode_count) *encode_count = entry->encode_count;
    if (error_count) *error_count = entry->error_count;

    return true;
}

void subghz_protocol_registry_print_stats(void) {
    if (!g_registry.initialized) {
        ESP_LOGW(TAG, "Registry not initialized");
        return;
    }

    ESP_LOGI(TAG, "=== Protocol Registry Statistics ===");
    ESP_LOGI(TAG, "Total protocols: %zu", g_registry.count);

    for (size_t i = 0; i < g_registry.count; i++) {
        SubGhzProtocolRegistryEntry *entry = &g_registry.entries[i];
        ESP_LOGI(
            TAG,
            "%s: %s, decode=%u, encode=%u, errors=%u",
            entry->definition.name,
            entry->definition.enabled ? "enabled" : "disabled",
            entry->decode_count,
            entry->encode_count,
            entry->error_count
        );
    }

    ESP_LOGI(TAG, "=====================================");
}

size_t subghz_protocol_registry_register_builtin(void) {
    size_t registered = 0;

    // Register Princeton protocol
    SubGhzProtocolDefinition princeton_def = {
        .name = "Princeton",
        .type = SubGhzProtocolTypeStatic,
        .frequency = 433920000,
        .preset = SubGhzPresetOok650Async,
        .interface =
            {
                        .decoder_alloc = (SubGhzProtocolDecoder * (*)(void)) subghz_protocol_decoder_princeton_alloc,
                        .decoder_free = (void (*)(SubGhzProtocolDecoder *))subghz_protocol_decoder_princeton_free,
                        .decoder_reset = (void (*)(SubGhzProtocolDecoder *))subghz_protocol_decoder_princeton_reset,
                        .decoder_feed =
                    (void (*)(SubGhzProtocolDecoder *, bool, uint32_t))subghz_protocol_decoder_princeton_feed,
                        .decoder_get_data = (bool (*)(SubGhzProtocolDecoder *, SubGhzProtocolConfig *))
                    subghz_protocol_decoder_princeton_get_data, .encoder_alloc = (SubGhzProtocolEncoder * (*)(void)) subghz_protocol_encoder_princeton_alloc,
                        .encoder_free = (void (*)(SubGhzProtocolEncoder *))subghz_protocol_encoder_princeton_free,
                        .encoder_set_data = (bool (*)(SubGhzProtocolEncoder *, SubGhzProtocolConfig *))
                    subghz_protocol_encoder_princeton_set_data, .encoder_get_upload = (uint32_t *(*)(SubGhzProtocolEncoder *,
                        size_t *))subghz_protocol_encoder_princeton_get_upload,
                        },
        .enabled = true
    };

    if (subghz_protocol_registry_register(&princeton_def)) { registered++; }

    // Register CAME protocol
    SubGhzProtocolDefinition came_def = {
        .name = "CAME",
        .type = SubGhzProtocolTypeStatic,
        .frequency = 433920000,
        .preset = SubGhzPresetOok650Async,
        .interface =
            {
                        .decoder_alloc = (SubGhzProtocolDecoder * (*)(void)) subghz_protocol_decoder_came_alloc,
                        .decoder_free = (void (*)(SubGhzProtocolDecoder *))subghz_protocol_decoder_came_free,
                        .decoder_reset = (void (*)(SubGhzProtocolDecoder *))subghz_protocol_decoder_came_reset,
                        .decoder_feed =
                    (void (*)(SubGhzProtocolDecoder *, bool, uint32_t))subghz_protocol_decoder_came_feed,
                        .decoder_get_data = (bool (*)(SubGhzProtocolDecoder *, SubGhzProtocolConfig *))
                    subghz_protocol_decoder_came_get_data, .encoder_alloc = (SubGhzProtocolEncoder * (*)(void)) subghz_protocol_encoder_came_alloc,
                        .encoder_free = (void (*)(SubGhzProtocolEncoder *))subghz_protocol_encoder_came_free,
                        .encoder_set_data = (bool (*)(SubGhzProtocolEncoder *, SubGhzProtocolConfig *))
                    subghz_protocol_encoder_came_set_data, .encoder_get_upload =
                    (uint32_t *(*)(SubGhzProtocolEncoder *, size_t *))subghz_protocol_encoder_came_get_upload,
                        },
        .enabled = true
    };
    if (subghz_protocol_registry_register(&came_def)) { registered++; }

    // Register KeeLoq protocol
    SubGhzProtocolDefinition keeloq_def = {
        .name = "KeeLoq",
        .type = SubGhzProtocolTypeDynamic,
        .frequency = 433920000,
        .preset = SubGhzPresetOok650Async,
        .interface =
            {
                        .decoder_alloc = (SubGhzProtocolDecoder * (*)(void)) subghz_protocol_decoder_keeloq_alloc,
                        .decoder_free = (void (*)(SubGhzProtocolDecoder *))subghz_protocol_decoder_keeloq_free,
                        .decoder_reset = (void (*)(SubGhzProtocolDecoder *))subghz_protocol_decoder_keeloq_reset,
                        .decoder_feed =
                    (void (*)(SubGhzProtocolDecoder *, bool, uint32_t))subghz_protocol_decoder_keeloq_feed,
                        .decoder_get_data = (bool (*)(SubGhzProtocolDecoder *, SubGhzProtocolConfig *))
                    subghz_protocol_decoder_keeloq_get_data, .encoder_alloc = (SubGhzProtocolEncoder * (*)(void)) subghz_protocol_encoder_keeloq_alloc,
                        .encoder_free = (void (*)(SubGhzProtocolEncoder *))subghz_protocol_encoder_keeloq_free,
                        .encoder_set_data = (bool (*)(SubGhzProtocolEncoder *, SubGhzProtocolConfig *))
                    subghz_protocol_encoder_keeloq_set_data, .encoder_get_upload = (uint32_t *(*)(SubGhzProtocolEncoder *,
                        size_t *))subghz_protocol_encoder_keeloq_get_upload,
                        },
        .enabled = true
    };
    if (subghz_protocol_registry_register(&keeloq_def)) { registered++; }

    // Register Somfy protocol
    SubGhzProtocolDefinition somfy_def = {
        .name = "Somfy",
        .type = SubGhzProtocolTypeDynamic,
        .frequency = 433420000,
        .preset = SubGhzPresetOok650Async,
        .interface =
            {
                        .decoder_alloc = (SubGhzProtocolDecoder * (*)(void)) subghz_protocol_decoder_somfy_alloc,
                        .decoder_free = (void (*)(SubGhzProtocolDecoder *))subghz_protocol_decoder_somfy_free,
                        .decoder_reset = (void (*)(SubGhzProtocolDecoder *))subghz_protocol_decoder_somfy_reset,
                        .decoder_feed =
                    (void (*)(SubGhzProtocolDecoder *, bool, uint32_t))subghz_protocol_decoder_somfy_feed,
                        .decoder_get_data = (bool (*)(SubGhzProtocolDecoder *, SubGhzProtocolConfig *))
                    subghz_protocol_decoder_somfy_get_data, .encoder_alloc = (SubGhzProtocolEncoder * (*)(void)) subghz_protocol_encoder_somfy_alloc,
                        .encoder_free = (void (*)(SubGhzProtocolEncoder *))subghz_protocol_encoder_somfy_free,
                        .encoder_set_data = (bool (*)(SubGhzProtocolEncoder *, SubGhzProtocolConfig *))
                    subghz_protocol_encoder_somfy_set_data, .encoder_get_upload = (uint32_t *(*)(SubGhzProtocolEncoder *,
                        size_t *))subghz_protocol_encoder_somfy_get_upload,
                        },
        .enabled = true
    };
    if (subghz_protocol_registry_register(&somfy_def)) { registered++; }

    // Future protocols can be added here:
    // - RAW protocol
    // - Star Line protocol
    // - Nice FLO protocol
    // etc.

    ESP_LOGI(TAG, "Registered %zu built-in protocols", registered);
    return registered;
}

// Additional functions needed by receiver/transmitter
bool subghz_protocol_registry_reset_all(void) {
    if (!g_registry.initialized) { return false; }

    // Reset all protocol states
    for (size_t i = 0; i < g_registry.count; i++) {
        SubGhzProtocolRegistryEntry *entry = &g_registry.entries[i];
        entry->decode_count = 0;
        entry->encode_count = 0;
        entry->error_count = 0;

        // If protocol has reset function, call it
        if (entry->definition.interface.decoder_reset) {
            // Reset would need decoder instance - for now just clear counters
        }
    }

    ESP_LOGD(TAG, "Reset all protocol states");
    return true;
}

bool subghz_protocol_registry_feed_data(const uint8_t *data, size_t size) {
    if (!g_registry.initialized || !data || size == 0) { return false; }

    // Feed data to all registered protocols for decoding
    bool decoded = false;

    for (size_t i = 0; i < g_registry.count; i++) {
        SubGhzProtocolRegistryEntry *entry = &g_registry.entries[i];

        // For now, this is a placeholder - actual implementation would
        // need to instantiate decoders and feed the data through them
        if (entry->definition.interface.decoder_feed) {
            // Try to decode with this protocol
            // This is a simplified version - real implementation would be more complex
        }
    }

    return decoded;
}
