#ifndef __SUBGHZ_PROTOCOL_KEELOQ_H__
#define __SUBGHZ_PROTOCOL_KEELOQ_H__

#include "subghz_protocol_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief KeeLoq Protocol Implementation
 *
 * KeeLoq is a rolling code protocol used by many car remotes and garage door openers
 * Operating frequency: 433.92 MHz (varies by region)
 * Modulation: OOK/ASK
 * Data bits: 66 bits (32-bit encrypted + 28-bit fixed + 6-bit function)
 * Encryption: KeeLoq block cipher
 */

// KeeLoq protocol specific definitions
#define SUBGHZ_PROTOCOL_KEELOQ_NAME "KeeLoq"
#define SUBGHZ_PROTOCOL_KEELOQ_FREQUENCY 433920000
#define SUBGHZ_PROTOCOL_KEELOQ_DATA_BITS 66
#define SUBGHZ_PROTOCOL_KEELOQ_TE_SHORT 400     // microseconds
#define SUBGHZ_PROTOCOL_KEELOQ_TE_LONG 800      // microseconds
#define SUBGHZ_PROTOCOL_KEELOQ_TE_TOLERANCE 100 // microseconds
#define SUBGHZ_PROTOCOL_KEELOQ_GUARD_TIME 10000 // microseconds
#define SUBGHZ_PROTOCOL_KEELOQ_PREAMBLE_BITS 12

// KeeLoq decoder
typedef struct SubGhzProtocolDecoderKeeloq SubGhzProtocolDecoderKeeloq;

/**
 * @brief Allocate KeeLoq decoder
 * @return Decoder instance
 */
SubGhzProtocolDecoderKeeloq *subghz_protocol_decoder_keeloq_alloc(void);

/**
 * @brief Free KeeLoq decoder
 * @param decoder Decoder instance
 */
void subghz_protocol_decoder_keeloq_free(SubGhzProtocolDecoderKeeloq *decoder);

/**
 * @brief Reset KeeLoq decoder
 * @param decoder Decoder instance
 */
void subghz_protocol_decoder_keeloq_reset(SubGhzProtocolDecoderKeeloq *decoder);

/**
 * @brief Feed data to KeeLoq decoder
 * @param decoder Decoder instance
 * @param level Signal level
 * @param duration Signal duration in microseconds
 */
void subghz_protocol_decoder_keeloq_feed(SubGhzProtocolDecoderKeeloq *decoder, bool level, uint32_t duration);

/**
 * @brief Get decoded data from KeeLoq decoder
 * @param decoder Decoder instance
 * @param config Output configuration
 * @return true if data is available
 */
bool subghz_protocol_decoder_keeloq_get_data(
    SubGhzProtocolDecoderKeeloq *decoder, SubGhzProtocolConfig *config
);

// KeeLoq encoder
typedef struct SubGhzProtocolEncoderKeeloq SubGhzProtocolEncoderKeeloq;

/**
 * @brief Allocate KeeLoq encoder
 * @return Encoder instance
 */
SubGhzProtocolEncoderKeeloq *subghz_protocol_encoder_keeloq_alloc(void);

/**
 * @brief Free KeeLoq encoder
 * @param encoder Encoder instance
 */
void subghz_protocol_encoder_keeloq_free(SubGhzProtocolEncoderKeeloq *encoder);

/**
 * @brief Set data for KeeLoq encoder
 * @param encoder Encoder instance
 * @param config Protocol configuration
 * @return true on success
 */
bool subghz_protocol_encoder_keeloq_set_data(
    SubGhzProtocolEncoderKeeloq *encoder, SubGhzProtocolConfig *config
);

/**
 * @brief Get upload data from KeeLoq encoder
 * @param encoder Encoder instance
 * @param upload_count Output upload count
 * @return Upload data array
 */
uint32_t *
subghz_protocol_encoder_keeloq_get_upload(SubGhzProtocolEncoderKeeloq *encoder, size_t *upload_count);

// KeeLoq specific functions
/**
 * @brief Decrypt KeeLoq data
 * @param encrypted Encrypted 32-bit data
 * @param key 64-bit manufacturer key
 * @return Decrypted 32-bit data
 */
uint32_t subghz_protocol_keeloq_decrypt(uint32_t encrypted, uint64_t key);

/**
 * @brief Encrypt KeeLoq data
 * @param data 32-bit data to encrypt
 * @param key 64-bit manufacturer key
 * @return Encrypted 32-bit data
 */
uint32_t subghz_protocol_keeloq_encrypt(uint32_t data, uint64_t key);

#ifdef __cplusplus
}
#endif

#endif // __SUBGHZ_PROTOCOL_KEELOQ_H__
