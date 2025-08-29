#include "subghz/subghz_protocol_princeton.h"
#include "subghz/subghz_memory.h"
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "Princeton";

// Helper function to check if duration matches expected with tolerance
static bool duration_check(uint32_t duration, uint32_t expected, uint32_t tolerance) {
    return (duration >= (expected - tolerance)) && (duration <= (expected + tolerance));
}

SubGhzProtocolDecoderPrinceton *subghz_protocol_decoder_princeton_alloc(void) {
    SubGhzProtocolDecoderPrinceton *instance = (SubGhzProtocolDecoderPrinceton *)subghz_malloc(
        SUBGHZ_POOL_PROTOCOL, sizeof(SubGhzProtocolDecoderPrinceton), SUBGHZ_MEM_FLAG_ZERO
    );

    if (instance) {
        instance->base.protocol_name = PRINCETON_PROTOCOL_NAME;
        instance->base.te_last = 0;
        instance->base.header_count = 0;
        subghz_protocol_decoder_princeton_reset(instance);
        ESP_LOGI(TAG, "Princeton decoder allocated");
    }

    return instance;
}

void subghz_protocol_decoder_princeton_free(SubGhzProtocolDecoderPrinceton *instance) {
    if (instance) {
        subghz_free(SUBGHZ_POOL_PROTOCOL, instance);
        ESP_LOGI(TAG, "Princeton decoder freed");
    }
}

void subghz_protocol_decoder_princeton_reset(SubGhzProtocolDecoderPrinceton *instance) {
    if (!instance) return;

    instance->base.decoder_step = SubGhzDecoderStepReset;
    instance->te_last = 0;
    instance->header_count = 0;
    instance->data = 0;
    instance->data_count_bit = 0;
    instance->decode_data = false;
    instance->decode_count_bit = false;
}

void subghz_protocol_decoder_princeton_feed(
    SubGhzProtocolDecoderPrinceton *instance, bool level, uint32_t duration
) {
    if (!instance) return;

    switch (instance->base.decoder_step) {
        case SubGhzDecoderStepReset:
            if ((!level) && duration_check(duration, PRINCETON_TE_SHORT * 36, PRINCETON_TE_DELTA * 10)) {
                // Found sync pattern
                instance->base.decoder_step = SubGhzDecoderStepFoundPreambula;
                instance->te_last = duration;
                instance->header_count = 0;
                ESP_LOGD(TAG, "Found sync pattern, duration: %u", duration);
            }
            break;

        case SubGhzDecoderStepFoundPreambula:
            if (level) {
                instance->te_last = duration;
                instance->base.decoder_step = SubGhzDecoderStepSaveDuration;
            } else {
                instance->base.decoder_step = SubGhzDecoderStepReset;
            }
            break;

        case SubGhzDecoderStepSaveDuration:
            if (!level) {
                if (duration >= (PRINCETON_TE_SHORT * 10 + PRINCETON_TE_DELTA)) {
                    // End of transmission
                    instance->base.decoder_step = SubGhzDecoderStepReset;
                    if ((instance->data_count_bit == PRINCETON_MIN_COUNT_BIT_FOR_FOUND) && instance->data) {
                        instance->decode_data = true;
                        instance->decode_count_bit = instance->data_count_bit;
                        ESP_LOGI(
                            TAG,
                            "Princeton decoded: 0x%08X (%u bits)",
                            instance->data,
                            instance->data_count_bit
                        );
                    }
                    break;
                }

                // Check for short pulse (0 bit)
                if (duration_check(instance->te_last, PRINCETON_TE_SHORT, PRINCETON_TE_DELTA) &&
                    duration_check(duration, PRINCETON_TE_LONG, PRINCETON_TE_DELTA)) {
                    // This is a '0' bit
                    instance->data = (instance->data << 1) | 0;
                    instance->data_count_bit++;
                    instance->base.decoder_step = SubGhzDecoderStepFoundPreambula;
                    ESP_LOGD(TAG, "Decoded bit: 0");
                }
                // Check for long pulse (1 bit)
                else if (duration_check(instance->te_last, PRINCETON_TE_LONG, PRINCETON_TE_DELTA) &&
                         duration_check(duration, PRINCETON_TE_SHORT, PRINCETON_TE_DELTA)) {
                    // This is a '1' bit
                    instance->data = (instance->data << 1) | 1;
                    instance->data_count_bit++;
                    instance->base.decoder_step = SubGhzDecoderStepFoundPreambula;
                    ESP_LOGD(TAG, "Decoded bit: 1");
                } else {
                    // Invalid duration, reset
                    instance->base.decoder_step = SubGhzDecoderStepReset;
                }
            } else {
                instance->base.decoder_step = SubGhzDecoderStepReset;
            }
            break;

        default: instance->base.decoder_step = SubGhzDecoderStepReset; break;
    }
}

bool subghz_protocol_decoder_princeton_get_data(
    SubGhzProtocolDecoderPrinceton *instance, SubGhzProtocolConfig *config
) {
    if (!instance || !config || !instance->decode_data) { return false; }

    // Clear config
    memset(config, 0, sizeof(SubGhzProtocolConfig));

    // Fill protocol info
    strncpy(config->name, PRINCETON_PROTOCOL_NAME, SUBGHZ_PROTOCOL_NAME_SIZE - 1);
    config->type = SubGhzProtocolTypeStatic;
    config->frequency = 433920000; // Default frequency in Hz
    config->preset = SubGhzPresetOok650Async;
    config->bit_count = instance->decode_count_bit;
    config->key = instance->data;

    // Extract serial and button from key (Princeton format)
    config->serial = (instance->data >> 4) & 0xFFFFF; // 20 bits
    config->btn = instance->data & 0xF;               // 4 bits

    // Reset decode flag
    instance->decode_data = false;

    ESP_LOGI(
        TAG,
        "Princeton data extracted: serial=0x%05X, btn=%u, key=0x%06X",
        config->serial,
        config->btn,
        (uint32_t)config->key
    );

    return true;
}

SubGhzProtocolEncoderPrinceton *subghz_protocol_encoder_princeton_alloc(void) {
    SubGhzProtocolEncoderPrinceton *instance = (SubGhzProtocolEncoderPrinceton *)subghz_malloc(
        SUBGHZ_POOL_PROTOCOL, sizeof(SubGhzProtocolEncoderPrinceton), SUBGHZ_MEM_FLAG_ZERO
    );

    if (instance) {
        instance->te_short = PRINCETON_TE_SHORT;
        instance->te_long = PRINCETON_TE_LONG;
        instance->repeat = 10; // Default repeat count
        ESP_LOGI(TAG, "Princeton encoder allocated");
    }

    return instance;
}

void subghz_protocol_encoder_princeton_free(SubGhzProtocolEncoderPrinceton *instance) {
    if (instance) {
        if (instance->upload) { subghz_free(SUBGHZ_POOL_SIGNAL, instance->upload); }
        subghz_free(SUBGHZ_POOL_PROTOCOL, instance);
        ESP_LOGI(TAG, "Princeton encoder freed");
    }
}

bool subghz_protocol_encoder_princeton_set_data(
    SubGhzProtocolEncoderPrinceton *instance, SubGhzProtocolConfig *config
) {
    if (!instance || !config) { return false; }

    // Store key data
    instance->key = (uint32_t)config->key;
    instance->serial = config->serial;
    instance->btn = config->btn;

    // Calculate upload array size
    // Each bit = 2 durations (high + low)
    // Sync = 2 durations
    // Total = (bit_count * 2) + 2 + end_gap
    size_t upload_size = (config->bit_count * 2) + 2 + 1;

    // Free previous upload if exists
    if (instance->upload) { subghz_free(SUBGHZ_POOL_SIGNAL, instance->upload); }

    // Allocate new upload array
    instance->upload =
        (uint32_t *)subghz_malloc(SUBGHZ_POOL_SIGNAL, upload_size * sizeof(uint32_t), SUBGHZ_MEM_FLAG_ZERO);
    if (!instance->upload) {
        ESP_LOGE(TAG, "Failed to allocate upload array");
        return false;
    }

    instance->upload_count = upload_size;

    size_t index = 0;

    // Add sync pulse (36 * TE_SHORT low)
    instance->upload[index++] = instance->te_short * 36;
    instance->upload[index++] = instance->te_short;

    // Encode data bits (MSB first)
    for (int i = config->bit_count - 1; i >= 0; i--) {
        bool bit = (instance->key >> i) & 1;

        if (bit) {
            // '1' bit: long high, short low
            instance->upload[index++] = instance->te_long;
            instance->upload[index++] = instance->te_short;
        } else {
            // '0' bit: short high, long low
            instance->upload[index++] = instance->te_short;
            instance->upload[index++] = instance->te_long;
        }
    }

    // Add end gap
    instance->upload[index++] = instance->te_short * 40;

    ESP_LOGI(
        TAG, "Princeton encoder data set: key=0x%08X, upload_count=%zu", instance->key, instance->upload_count
    );

    return true;
}

uint32_t *
subghz_protocol_encoder_princeton_get_upload(SubGhzProtocolEncoderPrinceton *instance, size_t *upload_count) {
    if (!instance || !upload_count) { return NULL; }

    *upload_count = instance->upload_count;
    return instance->upload;
}

bool subghz_protocol_princeton_create_config(
    SubGhzProtocolConfig *config, uint32_t key, uint32_t serial, uint8_t btn
) {
    if (!config) { return false; }

    // Clear config
    memset(config, 0, sizeof(SubGhzProtocolConfig));

    // Fill protocol info
    strncpy(config->name, PRINCETON_PROTOCOL_NAME, SUBGHZ_PROTOCOL_NAME_SIZE - 1);
    config->type = SubGhzProtocolTypeStatic;
    config->frequency = 433920000; // Default frequency in Hz
    config->preset = SubGhzPresetOok650Async;
    config->bit_count = PRINCETON_BIT_COUNT;

    // For Princeton, key can be provided directly or constructed from serial+btn
    if (key != 0) {
        config->key = key;
        config->serial = (key >> 4) & 0xFFFFF;
        config->btn = key & 0xF;
    } else {
        // Construct key from serial and button
        config->serial = serial & 0xFFFFF; // 20 bits max
        config->btn = btn & 0xF;           // 4 bits max
        config->key = (config->serial << 4) | config->btn;
    }

    ESP_LOGI(
        TAG,
        "Princeton config created: serial=0x%05X, btn=%u, key=0x%06X",
        config->serial,
        config->btn,
        (uint32_t)config->key
    );

    return true;
}
