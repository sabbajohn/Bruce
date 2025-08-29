#ifndef __SUBGHZ_PROTOCOL_REGISTRY_H__
#define __SUBGHZ_PROTOCOL_REGISTRY_H__

#include "subghz_memory.h"
#include "subghz_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SubGHz protocol registry system
 *
 * Manages registration, loading and operation of SubGHz protocols
 */

// Maximum number of registered protocols
#define SUBGHZ_PROTOCOL_MAX_COUNT 32

// Protocol decoder callback
typedef void (*SubGhzProtocolDecoderCallback)(void *context, SubGhzProtocolConfig *config);

// Protocol encoder interface
typedef struct SubGhzProtocolEncoder SubGhzProtocolEncoder;

// Protocol decoder interface
typedef struct SubGhzProtocolDecoder SubGhzProtocolDecoder;

// Protocol interface functions
typedef struct {
    // Decoder functions
    SubGhzProtocolDecoder *(*decoder_alloc)(void);
    void (*decoder_free)(SubGhzProtocolDecoder *decoder);
    void (*decoder_reset)(SubGhzProtocolDecoder *decoder);
    void (*decoder_feed)(SubGhzProtocolDecoder *decoder, bool level, uint32_t duration);
    bool (*decoder_get_data)(SubGhzProtocolDecoder *decoder, SubGhzProtocolConfig *config);

    // Encoder functions
    SubGhzProtocolEncoder *(*encoder_alloc)(void);
    void (*encoder_free)(SubGhzProtocolEncoder *encoder);
    bool (*encoder_set_data)(SubGhzProtocolEncoder *encoder, SubGhzProtocolConfig *config);
    uint32_t *(*encoder_get_upload)(SubGhzProtocolEncoder *encoder, size_t *upload_count);
} SubGhzProtocolInterface;

// Protocol definition
typedef struct {
    const char *name;
    SubGhzProtocolType type;
    uint32_t frequency;
    SubGhzPreset preset;
    SubGhzProtocolInterface interface;
    bool enabled;
} SubGhzProtocolDefinition;

// Protocol registry entry
typedef struct {
    SubGhzProtocolDefinition definition;
    SubGhzProtocolDecoder *decoder;
    SubGhzProtocolEncoder *encoder;
    uint32_t decode_count;
    uint32_t encode_count;
    uint32_t error_count;
} SubGhzProtocolRegistryEntry;

// Protocol registry
typedef struct {
    SubGhzProtocolRegistryEntry entries[SUBGHZ_PROTOCOL_MAX_COUNT];
    size_t count;
    bool initialized;
    SubGhzProtocolDecoderCallback decoder_callback;
    void *decoder_context;
} SubGhzProtocolRegistry;

/**
 * @brief Initialize protocol registry
 * @return true if successful
 */
bool subghz_protocol_registry_init(void);

/**
 * @brief Deinitialize protocol registry
 */
void subghz_protocol_registry_deinit(void);

/**
 * @brief Register a protocol
 * @param definition Protocol definition
 * @return true if successful
 */
bool subghz_protocol_registry_register(const SubGhzProtocolDefinition *definition);

/**
 * @brief Unregister a protocol
 * @param name Protocol name
 * @return true if successful
 */
bool subghz_protocol_registry_unregister(const char *name);

/**
 * @brief Get protocol by name
 * @param name Protocol name
 * @return Protocol entry or NULL if not found
 */
SubGhzProtocolRegistryEntry *subghz_protocol_registry_get_by_name(const char *name);

/**
 * @brief Get protocol by index
 * @param index Protocol index
 * @return Protocol entry or NULL if invalid index
 */
SubGhzProtocolRegistryEntry *subghz_protocol_registry_get_by_index(size_t index);

/**
 * @brief Get number of registered protocols
 * @return Number of protocols
 */
size_t subghz_protocol_registry_get_count(void);

/**
 * @brief Enable/disable protocol
 * @param name Protocol name
 * @param enabled Enable flag
 * @return true if successful
 */
bool subghz_protocol_registry_set_enabled(const char *name, bool enabled);

/**
 * @brief Check if protocol is enabled
 * @param name Protocol name
 * @return true if enabled
 */
bool subghz_protocol_registry_is_enabled(const char *name);

/**
 * @brief Set decoder callback for all protocols
 * @param callback Callback function
 * @param context Callback context
 */
void subghz_protocol_registry_set_decoder_callback(SubGhzProtocolDecoderCallback callback, void *context);

/**
 * @brief Reset all protocol decoders
 */
void subghz_protocol_registry_reset_decoders(void);

/**
 * @brief Feed data to all enabled protocol decoders
 * @param level Signal level
 * @param duration Duration in microseconds
 */
void subghz_protocol_registry_feed_decoders(bool level, uint32_t duration);

/**
 * @brief Create encoder for protocol
 * @param name Protocol name
 * @param config Protocol configuration
 * @return Encoder instance or NULL on failure
 */
SubGhzProtocolEncoder *
subghz_protocol_registry_create_encoder(const char *name, SubGhzProtocolConfig *config);

/**
 * @brief Free encoder
 * @param name Protocol name
 * @param encoder Encoder instance
 */
void subghz_protocol_registry_free_encoder(const char *name, SubGhzProtocolEncoder *encoder);

/**
 * @brief Get upload data from encoder
 * @param encoder Encoder instance
 * @param upload_count Output upload count
 * @return Upload data array or NULL
 */
uint32_t *subghz_protocol_registry_get_encoder_upload(SubGhzProtocolEncoder *encoder, size_t *upload_count);

/**
 * @brief Get protocol statistics
 * @param name Protocol name
 * @param decode_count Output decode count
 * @param encode_count Output encode count
 * @param error_count Output error count
 * @return true if successful
 */
bool subghz_protocol_registry_get_stats(
    const char *name, uint32_t *decode_count, uint32_t *encode_count, uint32_t *error_count
);

/**
 * @brief Print registry statistics
 */
void subghz_protocol_registry_print_stats(void);

/**
 * @brief Register built-in protocols
 * @return Number of protocols registered
 */
size_t subghz_protocol_registry_register_builtin(void);

/**
 * @brief Reset all protocol states
 * @return true if successful
 */
bool subghz_protocol_registry_reset_all(void);

/**
 * @brief Feed data to protocols for decoding
 * @param data Data buffer
 * @param size Data size
 * @return true if data was decoded by any protocol
 */
bool subghz_protocol_registry_feed_data(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif // __SUBGHZ_PROTOCOL_REGISTRY_H__
