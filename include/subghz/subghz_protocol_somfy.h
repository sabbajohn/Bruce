#ifndef __SUBGHZ_PROTOCOL_SOMFY_H__
#define __SUBGHZ_PROTOCOL_SOMFY_H__

#include "subghz_protocol_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Somfy Protocol Implementation
 *
 * Somfy RTS (Radio Technology Somfy) protocol for window blinds and awnings
 * Operating frequency: 433.42 MHz
 * Modulation: OOK (On-Off Keying)
 * Data bits: 56 bits (7 bytes)
 * Rolling code: Yes
 */

// Somfy protocol specific definitions
#define SUBGHZ_PROTOCOL_SOMFY_NAME "Somfy"
#define SUBGHZ_PROTOCOL_SOMFY_FREQUENCY 433420000
#define SUBGHZ_PROTOCOL_SOMFY_DATA_BITS 56
#define SUBGHZ_PROTOCOL_SOMFY_TE_SHORT 640     // microseconds
#define SUBGHZ_PROTOCOL_SOMFY_TE_LONG 1280     // microseconds
#define SUBGHZ_PROTOCOL_SOMFY_TE_TOLERANCE 200 // microseconds
#define SUBGHZ_PROTOCOL_SOMFY_SYNC_HIGH 2416   // microseconds
#define SUBGHZ_PROTOCOL_SOMFY_SYNC_LOW 2416    // microseconds
#define SUBGHZ_PROTOCOL_SOMFY_GUARD_TIME 30000 // microseconds

// Somfy commands
typedef enum {
    SOMFY_CMD_MY = 0x1,       // My/Stop
    SOMFY_CMD_UP = 0x2,       // Up
    SOMFY_CMD_MY_UP = 0x3,    // My + Up
    SOMFY_CMD_DOWN = 0x4,     // Down
    SOMFY_CMD_MY_DOWN = 0x5,  // My + Down
    SOMFY_CMD_UP_DOWN = 0x6,  // Up + Down
    SOMFY_CMD_PROG = 0x8,     // Programming
    SOMFY_CMD_SUN_FLAG = 0x9, // Sun + Flag
    SOMFY_CMD_FLAG = 0xA      // Flag
} SomfyCommand;

// Somfy decoder
typedef struct SubGhzProtocolDecoderSomfy SubGhzProtocolDecoderSomfy;

/**
 * @brief Allocate Somfy decoder
 * @return Decoder instance
 */
SubGhzProtocolDecoderSomfy *subghz_protocol_decoder_somfy_alloc(void);

/**
 * @brief Free Somfy decoder
 * @param decoder Decoder instance
 */
void subghz_protocol_decoder_somfy_free(SubGhzProtocolDecoderSomfy *decoder);

/**
 * @brief Reset Somfy decoder
 * @param decoder Decoder instance
 */
void subghz_protocol_decoder_somfy_reset(SubGhzProtocolDecoderSomfy *decoder);

/**
 * @brief Feed data to Somfy decoder
 * @param decoder Decoder instance
 * @param level Signal level
 * @param duration Signal duration in microseconds
 */
void subghz_protocol_decoder_somfy_feed(SubGhzProtocolDecoderSomfy *decoder, bool level, uint32_t duration);

/**
 * @brief Get decoded data from Somfy decoder
 * @param decoder Decoder instance
 * @param config Output configuration
 * @return true if data is available
 */
bool subghz_protocol_decoder_somfy_get_data(
    SubGhzProtocolDecoderSomfy *decoder, SubGhzProtocolConfig *config
);

// Somfy encoder
typedef struct SubGhzProtocolEncoderSomfy SubGhzProtocolEncoderSomfy;

/**
 * @brief Allocate Somfy encoder
 * @return Encoder instance
 */
SubGhzProtocolEncoderSomfy *subghz_protocol_encoder_somfy_alloc(void);

/**
 * @brief Free Somfy encoder
 * @param encoder Encoder instance
 */
void subghz_protocol_encoder_somfy_free(SubGhzProtocolEncoderSomfy *encoder);

/**
 * @brief Set data for Somfy encoder
 * @param encoder Encoder instance
 * @param config Protocol configuration
 * @return true on success
 */
bool subghz_protocol_encoder_somfy_set_data(
    SubGhzProtocolEncoderSomfy *encoder, SubGhzProtocolConfig *config
);

/**
 * @brief Get upload data from Somfy encoder
 * @param encoder Encoder instance
 * @param upload_count Output upload count
 * @return Upload data array
 */
uint32_t *subghz_protocol_encoder_somfy_get_upload(SubGhzProtocolEncoderSomfy *encoder, size_t *upload_count);

// Somfy specific functions
/**
 * @brief Calculate Somfy checksum
 * @param data Somfy data (first 6 bytes)
 * @return Checksum byte
 */
uint8_t subghz_protocol_somfy_calculate_checksum(const uint8_t *data);

/**
 * @brief Get command name
 * @param cmd Command code
 * @return Command name string
 */
const char *subghz_protocol_somfy_get_command_name(SomfyCommand cmd);

#ifdef __cplusplus
}
#endif

#endif // __SUBGHZ_PROTOCOL_SOMFY_H__
