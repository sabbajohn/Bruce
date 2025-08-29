#include "subghz/subghz_protocol_keeloq.h"
#include "subghz/subghz_memory.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "SubGhzKeeLoq";

// KeeLoq decoder state
typedef enum {
    KEELOQ_DECODER_STATE_RESET,
    KEELOQ_DECODER_STATE_PREAMBLE,
    KEELOQ_DECODER_STATE_DATA,
    KEELOQ_DECODER_STATE_COMPLETE
} KeeLoqDecoderState;

// KeeLoq decoder structure
struct SubGhzProtocolDecoderKeeloq {
    KeeLoqDecoderState state;
    uint64_t data;
    uint8_t bit_count;
    uint8_t preamble_count;
    uint32_t last_time;
    bool last_level;
    bool data_ready;

    // Decoded fields
    uint32_t encrypted_data;
    uint32_t fixed_data;
    uint8_t function_code;
};

// KeeLoq encoder structure
struct SubGhzProtocolEncoderKeeloq {
    uint64_t data;
    uint32_t encrypted_data;
    uint32_t fixed_data;
    uint8_t function_code;
    uint32_t *upload_data;
    size_t upload_count;
    size_t upload_index;
    bool data_set;
};

// KeeLoq cipher implementation (simplified)
static const uint8_t keeloq_nlf = 0x3A5C742E; // Non-linear function lookup

static uint32_t keeloq_nlf_func(uint32_t x) { return (keeloq_nlf >> (x & 0x1F)) & 1; }

uint32_t subghz_protocol_keeloq_decrypt(uint32_t encrypted, uint64_t key) {
    uint32_t x = encrypted;

    // KeeLoq decryption (528 rounds)
    for (int i = 0; i < 528; i++) {
        uint32_t r15 = (x >> 15) & 1;
        uint32_t r0 = x & 1;
        uint32_t keybit = (key >> (i & 63)) & 1;
        uint32_t nlf_out = keeloq_nlf_func(x);

        x >>= 1;
        x |= ((r15 ^ r0 ^ keybit ^ nlf_out) << 31);
    }

    return x;
}

uint32_t subghz_protocol_keeloq_encrypt(uint32_t data, uint64_t key) {
    uint32_t x = data;

    // KeeLoq encryption (528 rounds)
    for (int i = 0; i < 528; i++) {
        uint32_t r31 = (x >> 31) & 1;
        uint32_t r16 = (x >> 16) & 1;
        uint32_t keybit = (key >> ((527 - i) & 63)) & 1;
        uint32_t nlf_out = keeloq_nlf_func(x);

        x <<= 1;
        x |= (r31 ^ r16 ^ keybit ^ nlf_out);
    }

    return x;
}

// Helper functions
static bool keeloq_decode_bit(SubGhzProtocolDecoderKeeloq *decoder, bool level, uint32_t duration) {
    bool bit_value = false;
    bool valid = false;

    if (level) { // High level (PWM encoding)
        if (duration >= (SUBGHZ_PROTOCOL_KEELOQ_TE_SHORT - SUBGHZ_PROTOCOL_KEELOQ_TE_TOLERANCE) &&
            duration <= (SUBGHZ_PROTOCOL_KEELOQ_TE_SHORT + SUBGHZ_PROTOCOL_KEELOQ_TE_TOLERANCE)) {
            bit_value = false;
            valid = true;
        } else if (duration >= (SUBGHZ_PROTOCOL_KEELOQ_TE_LONG - SUBGHZ_PROTOCOL_KEELOQ_TE_TOLERANCE) &&
                   duration <= (SUBGHZ_PROTOCOL_KEELOQ_TE_LONG + SUBGHZ_PROTOCOL_KEELOQ_TE_TOLERANCE)) {
            bit_value = true;
            valid = true;
        }
    }

    if (valid && decoder->bit_count < SUBGHZ_PROTOCOL_KEELOQ_DATA_BITS) {
        decoder->data = (decoder->data << 1) | (bit_value ? 1 : 0);
        decoder->bit_count++;
        ESP_LOGV(TAG, "Bit %d: %d", decoder->bit_count, bit_value);
    }

    return valid;
}

static void keeloq_parse_data(SubGhzProtocolDecoderKeeloq *decoder) {
    // KeeLoq format: [Function:4][Fixed:28][Encrypted:32][Guard:2]
    decoder->function_code = (decoder->data >> 62) & 0x0F;
    decoder->fixed_data = (decoder->data >> 34) & 0x0FFFFFFF;
    decoder->encrypted_data = (decoder->data >> 2) & 0xFFFFFFFF;

    ESP_LOGI(
        TAG,
        "KeeLoq parsed - Function: 0x%X, Fixed: 0x%07X, Encrypted: 0x%08X",
        decoder->function_code,
        (unsigned int)decoder->fixed_data,
        (unsigned int)decoder->encrypted_data
    );
}

static void keeloq_generate_upload_data(SubGhzProtocolEncoderKeeloq *encoder) {
    if (!encoder || !encoder->data_set) return;

    // Calculate upload size: preamble + data bits + guard time
    size_t upload_size =
        (SUBGHZ_PROTOCOL_KEELOQ_PREAMBLE_BITS * 2) + (SUBGHZ_PROTOCOL_KEELOQ_DATA_BITS * 2) + 2;

    encoder->upload_data = (uint32_t *)subghz_malloc(SUBGHZ_POOL_TEMP, upload_size * sizeof(uint32_t), 0);
    if (!encoder->upload_data) {
        ESP_LOGE(TAG, "Failed to allocate upload data");
        return;
    }

    size_t index = 0;

    // Preamble (alternating pattern)
    for (int i = 0; i < SUBGHZ_PROTOCOL_KEELOQ_PREAMBLE_BITS; i++) {
        encoder->upload_data[index++] = SUBGHZ_PROTOCOL_KEELOQ_TE_SHORT; // High
        encoder->upload_data[index++] = SUBGHZ_PROTOCOL_KEELOQ_TE_SHORT; // Low
    }

    // Data bits (MSB first) - PWM encoding
    for (int i = SUBGHZ_PROTOCOL_KEELOQ_DATA_BITS - 1; i >= 0; i--) {
        bool bit = (encoder->data >> i) & 1;

        if (bit) {
            // Long pulse for '1'
            encoder->upload_data[index++] = SUBGHZ_PROTOCOL_KEELOQ_TE_LONG;  // High
            encoder->upload_data[index++] = SUBGHZ_PROTOCOL_KEELOQ_TE_SHORT; // Low
        } else {
            // Short pulse for '0'
            encoder->upload_data[index++] = SUBGHZ_PROTOCOL_KEELOQ_TE_SHORT; // High
            encoder->upload_data[index++] = SUBGHZ_PROTOCOL_KEELOQ_TE_LONG;  // Low
        }
    }

    // Guard time
    encoder->upload_data[index++] = SUBGHZ_PROTOCOL_KEELOQ_GUARD_TIME; // Long pause

    encoder->upload_count = index;
    ESP_LOGI(
        TAG, "Generated upload data: %zu samples for data 0x%016llX", encoder->upload_count, encoder->data
    );
}

// Decoder implementation
SubGhzProtocolDecoderKeeloq *subghz_protocol_decoder_keeloq_alloc(void) {
    SubGhzProtocolDecoderKeeloq *decoder = (SubGhzProtocolDecoderKeeloq *)subghz_malloc(
        SUBGHZ_POOL_PROTOCOL, sizeof(SubGhzProtocolDecoderKeeloq), 0
    );

    if (decoder) {
        subghz_protocol_decoder_keeloq_reset(decoder);
        ESP_LOGD(TAG, "KeeLoq decoder allocated");
    }

    return decoder;
}

void subghz_protocol_decoder_keeloq_free(SubGhzProtocolDecoderKeeloq *decoder) {
    if (decoder) {
        subghz_free(SUBGHZ_POOL_PROTOCOL, decoder);
        ESP_LOGD(TAG, "KeeLoq decoder freed");
    }
}

void subghz_protocol_decoder_keeloq_reset(SubGhzProtocolDecoderKeeloq *decoder) {
    if (!decoder) return;

    decoder->state = KEELOQ_DECODER_STATE_RESET;
    decoder->data = 0;
    decoder->bit_count = 0;
    decoder->preamble_count = 0;
    decoder->last_time = 0;
    decoder->last_level = false;
    decoder->data_ready = false;
    decoder->encrypted_data = 0;
    decoder->fixed_data = 0;
    decoder->function_code = 0;

    ESP_LOGV(TAG, "KeeLoq decoder reset");
}

void subghz_protocol_decoder_keeloq_feed(
    SubGhzProtocolDecoderKeeloq *decoder, bool level, uint32_t duration
) {
    if (!decoder) return;

    switch (decoder->state) {
        case KEELOQ_DECODER_STATE_RESET:
            if (level && duration >= SUBGHZ_PROTOCOL_KEELOQ_TE_SHORT) {
                decoder->state = KEELOQ_DECODER_STATE_PREAMBLE;
                decoder->preamble_count = 1;
                ESP_LOGV(TAG, "Starting preamble detection");
            }
            break;

        case KEELOQ_DECODER_STATE_PREAMBLE:
            if (duration >= (SUBGHZ_PROTOCOL_KEELOQ_TE_SHORT - SUBGHZ_PROTOCOL_KEELOQ_TE_TOLERANCE) &&
                duration <= (SUBGHZ_PROTOCOL_KEELOQ_TE_SHORT + SUBGHZ_PROTOCOL_KEELOQ_TE_TOLERANCE)) {
                decoder->preamble_count++;
                if (decoder->preamble_count >= SUBGHZ_PROTOCOL_KEELOQ_PREAMBLE_BITS * 2) {
                    decoder->state = KEELOQ_DECODER_STATE_DATA;
                    decoder->data = 0;
                    decoder->bit_count = 0;
                    ESP_LOGV(TAG, "Preamble found, starting data");
                }
            } else {
                decoder->state = KEELOQ_DECODER_STATE_RESET;
            }
            break;

        case KEELOQ_DECODER_STATE_DATA:
            if (keeloq_decode_bit(decoder, level, duration)) {
                if (decoder->bit_count >= SUBGHZ_PROTOCOL_KEELOQ_DATA_BITS) {
                    decoder->state = KEELOQ_DECODER_STATE_COMPLETE;
                    decoder->data_ready = true;
                    keeloq_parse_data(decoder);
                    ESP_LOGI(TAG, "KeeLoq data decoded: 0x%016llX", decoder->data);
                }
            } else {
                // Invalid timing, reset
                decoder->state = KEELOQ_DECODER_STATE_RESET;
                ESP_LOGV(TAG, "Invalid timing, reset");
            }
            break;

        case KEELOQ_DECODER_STATE_COMPLETE:
            // Wait for guard time or reset
            if (duration >= SUBGHZ_PROTOCOL_KEELOQ_GUARD_TIME) {
                decoder->state = KEELOQ_DECODER_STATE_RESET;
            }
            break;
    }

    decoder->last_level = level;
    decoder->last_time = duration;
}

bool subghz_protocol_decoder_keeloq_get_data(
    SubGhzProtocolDecoderKeeloq *decoder, SubGhzProtocolConfig *config
) {
    if (!decoder || !config || !decoder->data_ready) return false;

    config->name[0] = '\0';
    strncat(config->name, SUBGHZ_PROTOCOL_KEELOQ_NAME, sizeof(config->name) - 1);
    config->frequency = SUBGHZ_PROTOCOL_KEELOQ_FREQUENCY;
    config->preset = SubGhzPresetOok650Async;
    config->key = decoder->data;
    config->serial = decoder->fixed_data & 0xFFFFFFF; // Lower 28 bits
    config->btn = decoder->function_code;
    config->cnt = decoder->encrypted_data & 0xFFFF; // Counter from encrypted data (simplified)

    decoder->data_ready = false; // Data consumed
    return true;
}

// Encoder implementation
SubGhzProtocolEncoderKeeloq *subghz_protocol_encoder_keeloq_alloc(void) {
    SubGhzProtocolEncoderKeeloq *encoder = (SubGhzProtocolEncoderKeeloq *)subghz_malloc(
        SUBGHZ_POOL_PROTOCOL, sizeof(SubGhzProtocolEncoderKeeloq), 0
    );

    if (encoder) {
        memset(encoder, 0, sizeof(SubGhzProtocolEncoderKeeloq));
        ESP_LOGD(TAG, "KeeLoq encoder allocated");
    }

    return encoder;
}

void subghz_protocol_encoder_keeloq_free(SubGhzProtocolEncoderKeeloq *encoder) {
    if (encoder) {
        if (encoder->upload_data) { subghz_free(SUBGHZ_POOL_TEMP, encoder->upload_data); }
        subghz_free(SUBGHZ_POOL_PROTOCOL, encoder);
        ESP_LOGD(TAG, "KeeLoq encoder freed");
    }
}

bool subghz_protocol_encoder_keeloq_set_data(
    SubGhzProtocolEncoderKeeloq *encoder, SubGhzProtocolConfig *config
) {
    if (!encoder || !config) return false;

    // Build KeeLoq data: [Function:4][Fixed:28][Encrypted:32][Guard:2]
    encoder->function_code = config->btn & 0x0F;
    encoder->fixed_data = config->serial & 0x0FFFFFFF;
    encoder->encrypted_data = config->key & 0xFFFFFFFF;

    encoder->data = ((uint64_t)encoder->function_code << 62) | ((uint64_t)encoder->fixed_data << 34) |
                    ((uint64_t)encoder->encrypted_data << 2);

    encoder->data_set = true;

    // Generate upload data
    keeloq_generate_upload_data(encoder);

    ESP_LOGI(TAG, "KeeLoq encoder data set: 0x%016llX", encoder->data);
    return encoder->upload_data != NULL;
}

uint32_t *
subghz_protocol_encoder_keeloq_get_upload(SubGhzProtocolEncoderKeeloq *encoder, size_t *upload_count) {
    if (!encoder || !upload_count || !encoder->data_set) return NULL;

    *upload_count = encoder->upload_count;
    return encoder->upload_data;
}
