#include "subghz/subghz_protocol_somfy.h"
#include "subghz/subghz_memory.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "SubGhzSomfy";

// Somfy decoder state
typedef enum {
    SOMFY_DECODER_STATE_RESET,
    SOMFY_DECODER_STATE_SYNC,
    SOMFY_DECODER_STATE_SYNC_LOW,
    SOMFY_DECODER_STATE_DATA,
    SOMFY_DECODER_STATE_COMPLETE
} SomfyDecoderState;

// Somfy decoder structure
struct SubGhzProtocolDecoderSomfy {
    SomfyDecoderState state;
    uint64_t data;
    uint8_t bit_count;
    uint32_t last_time;
    bool last_level;
    bool data_ready;

    // Decoded fields
    uint8_t key;
    uint8_t ctrl;
    uint16_t rolling_code;
    uint32_t serial;
    uint8_t checksum;
};

// Somfy encoder structure
struct SubGhzProtocolEncoderSomfy {
    uint64_t data;
    uint8_t key;
    uint8_t ctrl;
    uint16_t rolling_code;
    uint32_t serial;
    uint32_t *upload_data;
    size_t upload_count;
    size_t upload_index;
    bool data_set;
};

// Command name lookup table
static const char *somfy_command_names[16] = {
    [0] = "Unknown",
    [1] = "My/Stop", // SOMFY_CMD_MY
    [2] = "Up",      // SOMFY_CMD_UP
    [3] = "My+Up",   // SOMFY_CMD_MY_UP
    [4] = "Down",    // SOMFY_CMD_DOWN
    [5] = "My+Down", // SOMFY_CMD_MY_DOWN
    [6] = "Up+Down", // SOMFY_CMD_UP_DOWN
    [7] = "Unknown",
    [8] = "Programming", // SOMFY_CMD_PROG
    [9] = "Sun+Flag",    // SOMFY_CMD_SUN_FLAG
    [10] = "Flag",       // SOMFY_CMD_FLAG
    [11] = "Unknown",
    [12] = "Unknown",
    [13] = "Unknown",
    [14] = "Unknown",
    [15] = "Unknown"
};

// Helper functions
uint8_t subghz_protocol_somfy_calculate_checksum(const uint8_t *data) {
    uint8_t checksum = 0;
    for (int i = 0; i < 6; i++) {
        checksum ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (checksum & 0x80) {
                checksum = (checksum << 1) ^ 0x07;
            } else {
                checksum <<= 1;
            }
        }
    }
    return checksum;
}

const char *subghz_protocol_somfy_get_command_name(SomfyCommand cmd) {
    if (cmd < sizeof(somfy_command_names) / sizeof(somfy_command_names[0]) && somfy_command_names[cmd]) {
        return somfy_command_names[cmd];
    }
    return "Unknown";
}

static bool somfy_decode_bit(SubGhzProtocolDecoderSomfy *decoder, bool level, uint32_t duration) {
    bool bit_value = false;
    bool valid = false;

    if (!level) { // Low level
        if (duration >= (SUBGHZ_PROTOCOL_SOMFY_TE_SHORT - SUBGHZ_PROTOCOL_SOMFY_TE_TOLERANCE) &&
            duration <= (SUBGHZ_PROTOCOL_SOMFY_TE_SHORT + SUBGHZ_PROTOCOL_SOMFY_TE_TOLERANCE)) {
            bit_value = false;
            valid = true;
        } else if (duration >= (SUBGHZ_PROTOCOL_SOMFY_TE_LONG - SUBGHZ_PROTOCOL_SOMFY_TE_TOLERANCE) &&
                   duration <= (SUBGHZ_PROTOCOL_SOMFY_TE_LONG + SUBGHZ_PROTOCOL_SOMFY_TE_TOLERANCE)) {
            bit_value = true;
            valid = true;
        }
    }

    if (valid && decoder->bit_count < SUBGHZ_PROTOCOL_SOMFY_DATA_BITS) {
        decoder->data = (decoder->data << 1) | (bit_value ? 1 : 0);
        decoder->bit_count++;
        ESP_LOGV(TAG, "Bit %d: %d", decoder->bit_count, bit_value);
    }

    return valid;
}

static void somfy_parse_data(SubGhzProtocolDecoderSomfy *decoder) {
    // Somfy format: [Key:8][Ctrl:4][Rolling:16][Serial:24][Checksum:8]
    uint8_t bytes[7];

    // Extract bytes from 56-bit data
    for (int i = 0; i < 7; i++) { bytes[i] = (decoder->data >> (8 * (6 - i))) & 0xFF; }

    // Parse fields
    decoder->key = bytes[0];
    decoder->ctrl = (bytes[1] >> 4) & 0x0F;
    decoder->rolling_code =
        ((uint16_t)(bytes[1] & 0x0F) << 12) | ((uint16_t)bytes[2] << 4) | ((bytes[3] >> 4) & 0x0F);
    decoder->serial = ((uint32_t)(bytes[3] & 0x0F) << 16) | ((uint32_t)bytes[4] << 8) | bytes[5];
    decoder->checksum = bytes[6];

    // Verify checksum
    uint8_t calculated_checksum = subghz_protocol_somfy_calculate_checksum(bytes);

    ESP_LOGI(
        TAG,
        "Somfy parsed - Key: 0x%02X, Ctrl: 0x%X (%s), RC: %d, Serial: 0x%06X, Checksum: 0x%02X %s",
        decoder->key,
        decoder->ctrl,
        subghz_protocol_somfy_get_command_name((SomfyCommand)decoder->ctrl),
        decoder->rolling_code,
        (unsigned int)decoder->serial,
        decoder->checksum,
        (calculated_checksum == decoder->checksum) ? "✓" : "✗"
    );
}

static void somfy_generate_upload_data(SubGhzProtocolEncoderSomfy *encoder) {
    if (!encoder || !encoder->data_set) return;

    // Calculate upload size: sync + data bits + guard time
    // Each frame is repeated 7 times with specific intervals
    size_t frame_size = 4 + (SUBGHZ_PROTOCOL_SOMFY_DATA_BITS * 2) + 2;
    size_t upload_size = frame_size * 7; // 7 repetitions

    encoder->upload_data = (uint32_t *)subghz_malloc(SUBGHZ_POOL_TEMP, upload_size * sizeof(uint32_t), 0);
    if (!encoder->upload_data) {
        ESP_LOGE(TAG, "Failed to allocate upload data");
        return;
    }

    size_t index = 0;

    // Generate 7 frames
    for (int frame = 0; frame < 7; frame++) {
        // Sync pattern
        encoder->upload_data[index++] = SUBGHZ_PROTOCOL_SOMFY_SYNC_HIGH; // High
        encoder->upload_data[index++] = SUBGHZ_PROTOCOL_SOMFY_SYNC_LOW;  // Low

        // Data bits (MSB first) - Manchester encoding
        for (int i = SUBGHZ_PROTOCOL_SOMFY_DATA_BITS - 1; i >= 0; i--) {
            bool bit = (encoder->data >> i) & 1;

            if (bit) {
                // '1' = short high + long low
                encoder->upload_data[index++] = SUBGHZ_PROTOCOL_SOMFY_TE_SHORT; // High
                encoder->upload_data[index++] = SUBGHZ_PROTOCOL_SOMFY_TE_LONG;  // Low
            } else {
                // '0' = long high + short low
                encoder->upload_data[index++] = SUBGHZ_PROTOCOL_SOMFY_TE_LONG;  // High
                encoder->upload_data[index++] = SUBGHZ_PROTOCOL_SOMFY_TE_SHORT; // Low
            }
        }

        // Inter-frame gap (varies by frame number)
        uint32_t gap = (frame < 6) ? SUBGHZ_PROTOCOL_SOMFY_GUARD_TIME : SUBGHZ_PROTOCOL_SOMFY_GUARD_TIME * 2;
        encoder->upload_data[index++] = gap;
    }

    encoder->upload_count = index;
    ESP_LOGI(
        TAG, "Generated upload data: %zu samples for data 0x%014llX", encoder->upload_count, encoder->data
    );
}

// Decoder implementation
SubGhzProtocolDecoderSomfy *subghz_protocol_decoder_somfy_alloc(void) {
    SubGhzProtocolDecoderSomfy *decoder = (SubGhzProtocolDecoderSomfy *)subghz_malloc(
        SUBGHZ_POOL_PROTOCOL, sizeof(SubGhzProtocolDecoderSomfy), 0
    );

    if (decoder) {
        subghz_protocol_decoder_somfy_reset(decoder);
        ESP_LOGD(TAG, "Somfy decoder allocated");
    }

    return decoder;
}

void subghz_protocol_decoder_somfy_free(SubGhzProtocolDecoderSomfy *decoder) {
    if (decoder) {
        subghz_free(SUBGHZ_POOL_PROTOCOL, decoder);
        ESP_LOGD(TAG, "Somfy decoder freed");
    }
}

void subghz_protocol_decoder_somfy_reset(SubGhzProtocolDecoderSomfy *decoder) {
    if (!decoder) return;

    decoder->state = SOMFY_DECODER_STATE_RESET;
    decoder->data = 0;
    decoder->bit_count = 0;
    decoder->last_time = 0;
    decoder->last_level = false;
    decoder->data_ready = false;
    decoder->key = 0;
    decoder->ctrl = 0;
    decoder->rolling_code = 0;
    decoder->serial = 0;
    decoder->checksum = 0;

    ESP_LOGV(TAG, "Somfy decoder reset");
}

void subghz_protocol_decoder_somfy_feed(SubGhzProtocolDecoderSomfy *decoder, bool level, uint32_t duration) {
    if (!decoder) return;

    switch (decoder->state) {
        case SOMFY_DECODER_STATE_RESET:
            if (level && duration >= SUBGHZ_PROTOCOL_SOMFY_SYNC_HIGH - SUBGHZ_PROTOCOL_SOMFY_TE_TOLERANCE &&
                duration <= SUBGHZ_PROTOCOL_SOMFY_SYNC_HIGH + SUBGHZ_PROTOCOL_SOMFY_TE_TOLERANCE) {
                decoder->state = SOMFY_DECODER_STATE_SYNC;
                ESP_LOGV(TAG, "Sync high detected");
            }
            break;

        case SOMFY_DECODER_STATE_SYNC:
            if (!level && duration >= SUBGHZ_PROTOCOL_SOMFY_SYNC_LOW - SUBGHZ_PROTOCOL_SOMFY_TE_TOLERANCE &&
                duration <= SUBGHZ_PROTOCOL_SOMFY_SYNC_LOW + SUBGHZ_PROTOCOL_SOMFY_TE_TOLERANCE) {
                decoder->state = SOMFY_DECODER_STATE_DATA;
                decoder->data = 0;
                decoder->bit_count = 0;
                ESP_LOGV(TAG, "Sync complete, starting data");
            } else {
                decoder->state = SOMFY_DECODER_STATE_RESET;
            }
            break;

        case SOMFY_DECODER_STATE_SYNC_LOW:
            // This state is currently unused but included for completeness
            decoder->state = SOMFY_DECODER_STATE_RESET;
            break;

        case SOMFY_DECODER_STATE_DATA:
            if (somfy_decode_bit(decoder, level, duration)) {
                if (decoder->bit_count >= SUBGHZ_PROTOCOL_SOMFY_DATA_BITS) {
                    decoder->state = SOMFY_DECODER_STATE_COMPLETE;
                    decoder->data_ready = true;
                    somfy_parse_data(decoder);
                    ESP_LOGI(TAG, "Somfy data decoded: 0x%014llX", decoder->data);
                }
            } else {
                // Invalid timing, reset
                decoder->state = SOMFY_DECODER_STATE_RESET;
                ESP_LOGV(TAG, "Invalid timing, reset");
            }
            break;

        case SOMFY_DECODER_STATE_COMPLETE:
            // Wait for guard time or reset
            if (duration >= SUBGHZ_PROTOCOL_SOMFY_GUARD_TIME) { decoder->state = SOMFY_DECODER_STATE_RESET; }
            break;
    }

    decoder->last_level = level;
    decoder->last_time = duration;
}

bool subghz_protocol_decoder_somfy_get_data(
    SubGhzProtocolDecoderSomfy *decoder, SubGhzProtocolConfig *config
) {
    if (!decoder || !config || !decoder->data_ready) return false;

    config->name[0] = '\0';
    strncat(config->name, SUBGHZ_PROTOCOL_SOMFY_NAME, sizeof(config->name) - 1);
    config->frequency = SUBGHZ_PROTOCOL_SOMFY_FREQUENCY;
    config->preset = SubGhzPresetOok650Async;
    config->key = decoder->data;
    config->serial = decoder->serial;
    config->btn = decoder->ctrl;
    config->cnt = decoder->rolling_code;

    decoder->data_ready = false; // Data consumed
    return true;
}

// Encoder implementation
SubGhzProtocolEncoderSomfy *subghz_protocol_encoder_somfy_alloc(void) {
    SubGhzProtocolEncoderSomfy *encoder = (SubGhzProtocolEncoderSomfy *)subghz_malloc(
        SUBGHZ_POOL_PROTOCOL, sizeof(SubGhzProtocolEncoderSomfy), 0
    );

    if (encoder) {
        memset(encoder, 0, sizeof(SubGhzProtocolEncoderSomfy));
        ESP_LOGD(TAG, "Somfy encoder allocated");
    }

    return encoder;
}

void subghz_protocol_encoder_somfy_free(SubGhzProtocolEncoderSomfy *encoder) {
    if (encoder) {
        if (encoder->upload_data) { subghz_free(SUBGHZ_POOL_TEMP, encoder->upload_data); }
        subghz_free(SUBGHZ_POOL_PROTOCOL, encoder);
        ESP_LOGD(TAG, "Somfy encoder freed");
    }
}

bool subghz_protocol_encoder_somfy_set_data(
    SubGhzProtocolEncoderSomfy *encoder, SubGhzProtocolConfig *config
) {
    if (!encoder || !config) return false;

    // Build Somfy data: [Key:8][Ctrl:4][Rolling:16][Serial:24][Checksum:8]
    encoder->key = (config->key >> 48) & 0xFF;
    encoder->ctrl = config->btn & 0x0F;
    encoder->rolling_code = config->cnt & 0xFFFF;
    encoder->serial = config->serial & 0xFFFFFF;

    // Build data without checksum first
    uint8_t bytes[7];
    bytes[0] = encoder->key;
    bytes[1] = (encoder->ctrl << 4) | ((encoder->rolling_code >> 12) & 0x0F);
    bytes[2] = (encoder->rolling_code >> 4) & 0xFF;
    bytes[3] = ((encoder->rolling_code & 0x0F) << 4) | ((encoder->serial >> 16) & 0x0F);
    bytes[4] = (encoder->serial >> 8) & 0xFF;
    bytes[5] = encoder->serial & 0xFF;

    // Calculate checksum
    bytes[6] = subghz_protocol_somfy_calculate_checksum(bytes);

    // Build final 56-bit data
    encoder->data = 0;
    for (int i = 0; i < 7; i++) { encoder->data = (encoder->data << 8) | bytes[i]; }

    encoder->data_set = true;

    // Generate upload data
    somfy_generate_upload_data(encoder);

    ESP_LOGI(TAG, "Somfy encoder data set: 0x%014llX", encoder->data);
    return encoder->upload_data != NULL;
}

uint32_t *
subghz_protocol_encoder_somfy_get_upload(SubGhzProtocolEncoderSomfy *encoder, size_t *upload_count) {
    if (!encoder || !upload_count || !encoder->data_set) return NULL;

    *upload_count = encoder->upload_count;
    return encoder->upload_data;
}
