#ifndef __SUBGHZ_PROTOCOL_PRINCETON_H__
#define __SUBGHZ_PROTOCOL_PRINCETON_H__

#include "subghz/subghz_core.h"
#include "subghz/subghz_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Princeton protocol implementation
 *
 * Basic implementation of Princeton protocol for SubGHz
 * This is a simple static protocol commonly used in garage door openers
 */

// Princeton protocol constants
#define PRINCETON_PROTOCOL_NAME "Princeton"
#define PRINCETON_BIT_COUNT 24
#define PRINCETON_TE_SHORT 400
#define PRINCETON_TE_LONG 1200
#define PRINCETON_TE_DELTA 150
#define PRINCETON_MIN_COUNT_BIT_FOR_FOUND 24

// Princeton decoder state
typedef struct {
    SubGhzDecoderState base;
    uint32_t te_last;
    uint8_t header_count;
    uint32_t data;
    uint32_t data_count_bit;
    bool decode_data;
    bool decode_count_bit;
} SubGhzProtocolDecoderPrinceton;

// Princeton encoder state
typedef struct {
    uint32_t *upload;
    size_t upload_count;
    uint32_t repeat;
    uint32_t te_short;
    uint32_t te_long;
    uint32_t key;
    uint32_t serial;
    uint8_t btn;
} SubGhzProtocolEncoderPrinceton;

/**
 * @brief Initialize Princeton decoder
 * @return Decoder instance or NULL on failure
 */
SubGhzProtocolDecoderPrinceton *subghz_protocol_decoder_princeton_alloc(void);

/**
 * @brief Free Princeton decoder
 * @param instance Decoder instance
 */
void subghz_protocol_decoder_princeton_free(SubGhzProtocolDecoderPrinceton *instance);

/**
 * @brief Reset Princeton decoder
 * @param instance Decoder instance
 */
void subghz_protocol_decoder_princeton_reset(SubGhzProtocolDecoderPrinceton *instance);

/**
 * @brief Feed data to Princeton decoder
 * @param instance Decoder instance
 * @param level Signal level (true = high, false = low)
 * @param duration Duration in microseconds
 */
void subghz_protocol_decoder_princeton_feed(
    SubGhzProtocolDecoderPrinceton *instance, bool level, uint32_t duration
);

/**
 * @brief Get decoded data from Princeton decoder
 * @param instance Decoder instance
 * @param config Output protocol configuration
 * @return true if data is available
 */
bool subghz_protocol_decoder_princeton_get_data(
    SubGhzProtocolDecoderPrinceton *instance, SubGhzProtocolConfig *config
);

/**
 * @brief Initialize Princeton encoder
 * @return Encoder instance or NULL on failure
 */
SubGhzProtocolEncoderPrinceton *subghz_protocol_encoder_princeton_alloc(void);

/**
 * @brief Free Princeton encoder
 * @param instance Encoder instance
 */
void subghz_protocol_encoder_princeton_free(SubGhzProtocolEncoderPrinceton *instance);

/**
 * @brief Set data for Princeton encoder
 * @param instance Encoder instance
 * @param config Protocol configuration
 * @return true if successful
 */
bool subghz_protocol_encoder_princeton_set_data(
    SubGhzProtocolEncoderPrinceton *instance, SubGhzProtocolConfig *config
);

/**
 * @brief Get upload data from Princeton encoder
 * @param instance Encoder instance
 * @param upload_count Output upload count
 * @return Upload data array
 */
uint32_t *
subghz_protocol_encoder_princeton_get_upload(SubGhzProtocolEncoderPrinceton *instance, size_t *upload_count);

/**
 * @brief Create Princeton protocol configuration
 * @param config Output configuration
 * @param key Protocol key (24-bit)
 * @param serial Serial number (20-bit)
 * @param btn Button code (4-bit)
 * @return true if successful
 */
bool subghz_protocol_princeton_create_config(
    SubGhzProtocolConfig *config, uint32_t key, uint32_t serial, uint8_t btn
);

#ifdef __cplusplus
}
#endif

#endif // __SUBGHZ_PROTOCOL_PRINCETON_H__
