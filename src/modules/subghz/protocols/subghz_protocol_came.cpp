#include "subghz/subghz_protocol_came.h"
#include "subghz/subghz_memory.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "SubGhzCAME";

// CAME decoder state
typedef enum {
    CAME_DECODER_STATE_RESET,
    CAME_DECODER_STATE_WAITING_SYNC,
    CAME_DECODER_STATE_DATA,
    CAME_DECODER_STATE_COMPLETE
} CAMEDecoderState;

// CAME decoder structure
struct SubGhzProtocolDecoderCame {
    CAMEDecoderState state;
    uint16_t data;
    uint8_t bit_count;
    uint32_t last_time;
    bool last_level;
    bool data_ready;
};

// CAME encoder structure
struct SubGhzProtocolEncoderCame {
    uint16_t data;
    uint32_t *upload_data;
    size_t upload_count;
    size_t upload_index;
    bool data_set;
};

// Helper functions
static bool came_decode_bit(SubGhzProtocolDecoderCame *decoder, bool level, uint32_t duration) {
    bool bit_value = false;
    bool valid = false;

    if (!level) { // Low level
        if (duration >= (SUBGHZ_PROTOCOL_CAME_TE_SHORT - SUBGHZ_PROTOCOL_CAME_TE_TOLERANCE) &&
            duration <= (SUBGHZ_PROTOCOL_CAME_TE_SHORT + SUBGHZ_PROTOCOL_CAME_TE_TOLERANCE)) {
            bit_value = false;
            valid = true;
        } else if (duration >= (SUBGHZ_PROTOCOL_CAME_TE_LONG - SUBGHZ_PROTOCOL_CAME_TE_TOLERANCE) &&
                   duration <= (SUBGHZ_PROTOCOL_CAME_TE_LONG + SUBGHZ_PROTOCOL_CAME_TE_TOLERANCE)) {
            bit_value = true;
            valid = true;
        }
    }

    if (valid && decoder->bit_count < SUBGHZ_PROTOCOL_CAME_DATA_BITS) {
        decoder->data = (decoder->data << 1) | (bit_value ? 1 : 0);
        decoder->bit_count++;
        ESP_LOGV(TAG, "Bit %d: %d (data: 0x%03X)", decoder->bit_count, bit_value, decoder->data);
    }

    return valid;
}

static void came_generate_upload_data(SubGhzProtocolEncoderCame *encoder) {
    if (!encoder || !encoder->data_set) return;

    // Calculate upload size: preamble + data bits + guard time
    size_t upload_size = 2 + (SUBGHZ_PROTOCOL_CAME_DATA_BITS * 2) + 2;

    encoder->upload_data = (uint32_t *)subghz_malloc(SUBGHZ_POOL_TEMP, upload_size * sizeof(uint32_t), 0);
    if (!encoder->upload_data) {
        ESP_LOGE(TAG, "Failed to allocate upload data");
        return;
    }

    size_t index = 0;

    // Preamble (sync)
    encoder->upload_data[index++] = SUBGHZ_PROTOCOL_CAME_TE_LONG;  // High
    encoder->upload_data[index++] = SUBGHZ_PROTOCOL_CAME_TE_SHORT; // Low

    // Data bits (MSB first)
    for (int i = SUBGHZ_PROTOCOL_CAME_DATA_BITS - 1; i >= 0; i--) {
        bool bit = (encoder->data >> i) & 1;

        if (bit) {
            // Long pulse for '1'
            encoder->upload_data[index++] = SUBGHZ_PROTOCOL_CAME_TE_LONG;  // High
            encoder->upload_data[index++] = SUBGHZ_PROTOCOL_CAME_TE_SHORT; // Low
        } else {
            // Short pulse for '0'
            encoder->upload_data[index++] = SUBGHZ_PROTOCOL_CAME_TE_SHORT; // High
            encoder->upload_data[index++] = SUBGHZ_PROTOCOL_CAME_TE_LONG;  // Low
        }
    }

    // Guard time
    encoder->upload_data[index++] = SUBGHZ_PROTOCOL_CAME_GUARD_TIME; // Long pause

    encoder->upload_count = index;
    ESP_LOGI(TAG, "Generated upload data: %zu samples for data 0x%03X", encoder->upload_count, encoder->data);
}

// Decoder implementation
SubGhzProtocolDecoderCame *subghz_protocol_decoder_came_alloc(void) {
    SubGhzProtocolDecoderCame *decoder = (SubGhzProtocolDecoderCame *)subghz_malloc(
        SUBGHZ_POOL_PROTOCOL, sizeof(SubGhzProtocolDecoderCame), 0
    );

    if (decoder) {
        subghz_protocol_decoder_came_reset(decoder);
        ESP_LOGD(TAG, "CAME decoder allocated");
    }

    return decoder;
}

void subghz_protocol_decoder_came_free(SubGhzProtocolDecoderCame *decoder) {
    if (decoder) {
        subghz_free(SUBGHZ_POOL_PROTOCOL, decoder);
        ESP_LOGD(TAG, "CAME decoder freed");
    }
}

void subghz_protocol_decoder_came_reset(SubGhzProtocolDecoderCame *decoder) {
    if (!decoder) return;

    decoder->state = CAME_DECODER_STATE_RESET;
    decoder->data = 0;
    decoder->bit_count = 0;
    decoder->last_time = 0;
    decoder->last_level = false;
    decoder->data_ready = false;

    ESP_LOGV(TAG, "CAME decoder reset");
}

void subghz_protocol_decoder_came_feed(SubGhzProtocolDecoderCame *decoder, bool level, uint32_t duration) {
    if (!decoder) return;

    switch (decoder->state) {
        case CAME_DECODER_STATE_RESET:
            if (level && duration >= SUBGHZ_PROTOCOL_CAME_TE_LONG) {
                decoder->state = CAME_DECODER_STATE_WAITING_SYNC;
                ESP_LOGV(TAG, "Waiting for sync");
            }
            break;

        case CAME_DECODER_STATE_WAITING_SYNC:
            if (!level && duration >= SUBGHZ_PROTOCOL_CAME_TE_SHORT) {
                decoder->state = CAME_DECODER_STATE_DATA;
                decoder->data = 0;
                decoder->bit_count = 0;
                ESP_LOGV(TAG, "Sync found, starting data");
            } else if (level && duration < SUBGHZ_PROTOCOL_CAME_TE_LONG) {
                decoder->state = CAME_DECODER_STATE_RESET;
            }
            break;

        case CAME_DECODER_STATE_DATA:
            if (came_decode_bit(decoder, level, duration)) {
                if (decoder->bit_count >= SUBGHZ_PROTOCOL_CAME_DATA_BITS) {
                    decoder->state = CAME_DECODER_STATE_COMPLETE;
                    decoder->data_ready = true;
                    ESP_LOGI(TAG, "CAME data decoded: 0x%03X (%d)", decoder->data, decoder->data);
                }
            } else {
                // Invalid timing, reset
                decoder->state = CAME_DECODER_STATE_RESET;
                ESP_LOGV(TAG, "Invalid timing, reset");
            }
            break;

        case CAME_DECODER_STATE_COMPLETE:
            // Wait for guard time or reset
            if (duration >= SUBGHZ_PROTOCOL_CAME_GUARD_TIME) { decoder->state = CAME_DECODER_STATE_RESET; }
            break;
    }

    decoder->last_level = level;
    decoder->last_time = duration;
}

bool subghz_protocol_decoder_came_get_data(SubGhzProtocolDecoderCame *decoder, SubGhzProtocolConfig *config) {
    if (!decoder || !config || !decoder->data_ready) return false;

    config->name[0] = '\0';
    strncat(config->name, SUBGHZ_PROTOCOL_CAME_NAME, sizeof(config->name) - 1);
    config->frequency = SUBGHZ_PROTOCOL_CAME_FREQUENCY;
    config->preset = SubGhzPresetOok650Async;
    config->key = decoder->data;
    config->serial = decoder->data & 0xFF;     // Lower 8 bits as serial
    config->btn = (decoder->data >> 8) & 0x0F; // Upper 4 bits as button
    config->cnt = 0;                           // CAME doesn't use rolling code

    decoder->data_ready = false; // Data consumed
    return true;
}

// Encoder implementation
SubGhzProtocolEncoderCame *subghz_protocol_encoder_came_alloc(void) {
    SubGhzProtocolEncoderCame *encoder = (SubGhzProtocolEncoderCame *)subghz_malloc(
        SUBGHZ_POOL_PROTOCOL, sizeof(SubGhzProtocolEncoderCame), 0
    );

    if (encoder) {
        memset(encoder, 0, sizeof(SubGhzProtocolEncoderCame));
        ESP_LOGD(TAG, "CAME encoder allocated");
    }

    return encoder;
}

void subghz_protocol_encoder_came_free(SubGhzProtocolEncoderCame *encoder) {
    if (encoder) {
        if (encoder->upload_data) { subghz_free(SUBGHZ_POOL_TEMP, encoder->upload_data); }
        subghz_free(SUBGHZ_POOL_PROTOCOL, encoder);
        ESP_LOGD(TAG, "CAME encoder freed");
    }
}

bool subghz_protocol_encoder_came_set_data(SubGhzProtocolEncoderCame *encoder, SubGhzProtocolConfig *config) {
    if (!encoder || !config) return false;

    // CAME uses 12-bit data
    encoder->data = config->key & 0xFFF;
    encoder->data_set = true;

    // Generate upload data
    came_generate_upload_data(encoder);

    ESP_LOGI(TAG, "CAME encoder data set: 0x%03X", encoder->data);
    return encoder->upload_data != NULL;
}

uint32_t *subghz_protocol_encoder_came_get_upload(SubGhzProtocolEncoderCame *encoder, size_t *upload_count) {
    if (!encoder || !upload_count || !encoder->data_set) return NULL;

    *upload_count = encoder->upload_count;
    return encoder->upload_data;
}
