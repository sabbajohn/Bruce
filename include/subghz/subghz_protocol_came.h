#ifndef __SUBGHZ_PROTOCOL_CAME_H__
#define __SUBGHZ_PROTOCOL_CAME_H__

#include "subghz_protocol_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CAME Protocol Implementation
 *
 * CAME is a 12-bit protocol used by CAME gate/garage door remotes
 * Operating frequency: 433.92 MHz
 * Modulation: OOK (On-Off Keying)
 * Data bits: 12 bits
 * Timing: ~320us short, ~640us long
 */

// CAME protocol specific definitions
#define SUBGHZ_PROTOCOL_CAME_NAME "CAME"
#define SUBGHZ_PROTOCOL_CAME_FREQUENCY 433920000
#define SUBGHZ_PROTOCOL_CAME_DATA_BITS 12
#define SUBGHZ_PROTOCOL_CAME_TE_SHORT 320    // microseconds
#define SUBGHZ_PROTOCOL_CAME_TE_LONG 640     // microseconds
#define SUBGHZ_PROTOCOL_CAME_TE_TOLERANCE 60 // microseconds
#define SUBGHZ_PROTOCOL_CAME_GUARD_TIME 5000 // microseconds

// CAME decoder
typedef struct SubGhzProtocolDecoderCame SubGhzProtocolDecoderCame;

/**
 * @brief Allocate CAME decoder
 * @return Decoder instance
 */
SubGhzProtocolDecoderCame *subghz_protocol_decoder_came_alloc(void);

/**
 * @brief Free CAME decoder
 * @param decoder Decoder instance
 */
void subghz_protocol_decoder_came_free(SubGhzProtocolDecoderCame *decoder);

/**
 * @brief Reset CAME decoder
 * @param decoder Decoder instance
 */
void subghz_protocol_decoder_came_reset(SubGhzProtocolDecoderCame *decoder);

/**
 * @brief Feed data to CAME decoder
 * @param decoder Decoder instance
 * @param level Signal level
 * @param duration Signal duration in microseconds
 */
void subghz_protocol_decoder_came_feed(SubGhzProtocolDecoderCame *decoder, bool level, uint32_t duration);

/**
 * @brief Get decoded data from CAME decoder
 * @param decoder Decoder instance
 * @param config Output configuration
 * @return true if data is available
 */
bool subghz_protocol_decoder_came_get_data(SubGhzProtocolDecoderCame *decoder, SubGhzProtocolConfig *config);

// CAME encoder
typedef struct SubGhzProtocolEncoderCame SubGhzProtocolEncoderCame;

/**
 * @brief Allocate CAME encoder
 * @return Encoder instance
 */
SubGhzProtocolEncoderCame *subghz_protocol_encoder_came_alloc(void);

/**
 * @brief Free CAME encoder
 * @param encoder Encoder instance
 */
void subghz_protocol_encoder_came_free(SubGhzProtocolEncoderCame *encoder);

/**
 * @brief Set data for CAME encoder
 * @param encoder Encoder instance
 * @param config Protocol configuration
 * @return true on success
 */
bool subghz_protocol_encoder_came_set_data(SubGhzProtocolEncoderCame *encoder, SubGhzProtocolConfig *config);

/**
 * @brief Get upload data from CAME encoder
 * @param encoder Encoder instance
 * @param upload_count Output upload count
 * @return Upload data array
 */
uint32_t *subghz_protocol_encoder_came_get_upload(SubGhzProtocolEncoderCame *encoder, size_t *upload_count);

#ifdef __cplusplus
}
#endif

#endif // __SUBGHZ_PROTOCOL_CAME_H__
