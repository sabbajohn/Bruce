#ifndef __SUBGHZ_RECEIVER_H__
#define __SUBGHZ_RECEIVER_H__

#include "subghz_device.h"
#include "subghz_protocol_registry.h"
#include "subghz_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SubGHz receiver implementation
 *
 * Handles signal reception with automatic protocol detection
 */

// Receiver instance
typedef struct SubGhzReceiver SubGhzReceiver;

// Receiver configuration
typedef struct {
    float frequency;
    SubGhzPreset preset;
    int32_t rssi_threshold;
    bool filter_enabled;
    bool raw_capture_enabled;
    uint32_t timeout_ms;
} SubGhzReceiverConfig;

// Signal detection callback
typedef void (*SubGhzReceiverCallback)(void *context, SubGhzProtocolConfig *protocol_config);

// Raw signal callback
typedef void (*SubGhzReceiverRawCallback)(void *context, bool level, uint32_t duration);

/**
 * @brief Create receiver instance
 * @param device SubGHz device to use
 * @return Receiver instance or NULL on failure
 */
SubGhzReceiver *subghz_receiver_create(SubGhzDevice *device);

/**
 * @brief Free receiver instance
 * @param receiver Receiver instance
 */
void subghz_receiver_free(SubGhzReceiver *receiver);

/**
 * @brief Configure receiver
 * @param receiver Receiver instance
 * @param config Configuration
 * @return true if successful
 */
bool subghz_receiver_configure(SubGhzReceiver *receiver, const SubGhzReceiverConfig *config);

/**
 * @brief Set protocol detection callback
 * @param receiver Receiver instance
 * @param callback Callback function
 * @param context Callback context
 */
void subghz_receiver_set_callback(SubGhzReceiver *receiver, SubGhzReceiverCallback callback, void *context);

/**
 * @brief Set raw signal callback
 * @param receiver Receiver instance
 * @param callback Callback function
 * @param context Callback context
 */
void subghz_receiver_set_raw_callback(
    SubGhzReceiver *receiver, SubGhzReceiverRawCallback callback, void *context
);

/**
 * @brief Start receiver
 * @param receiver Receiver instance
 * @return true if successful
 */
bool subghz_receiver_start(SubGhzReceiver *receiver);

/**
 * @brief Stop receiver
 * @param receiver Receiver instance
 */
void subghz_receiver_stop(SubGhzReceiver *receiver);

/**
 * @brief Check if receiver is running
 * @param receiver Receiver instance
 * @return true if running
 */
bool subghz_receiver_is_running(SubGhzReceiver *receiver);

/**
 * @brief Get current state
 * @param receiver Receiver instance
 * @return Current state
 */
SubGhzReceiverState subghz_receiver_get_state(SubGhzReceiver *receiver);

/**
 * @brief Get current RSSI
 * @param receiver Receiver instance
 * @return RSSI in dBm
 */
int32_t subghz_receiver_get_rssi(SubGhzReceiver *receiver);

/**
 * @brief Process receiver (call from main loop or ISR)
 * @param receiver Receiver instance
 */
void subghz_receiver_process(SubGhzReceiver *receiver);

/**
 * @brief Feed raw signal data to receiver
 * @param receiver Receiver instance
 * @param level Signal level (true = high, false = low)
 * @param duration Duration in microseconds
 */
void subghz_receiver_feed_data(SubGhzReceiver *receiver, bool level, uint32_t duration);

/**
 * @brief Reset receiver state
 * @param receiver Receiver instance
 */
void subghz_receiver_reset(SubGhzReceiver *receiver);

/**
 * @brief Get receiver statistics
 * @param receiver Receiver instance
 * @param packets_received Output packets received count
 * @param packets_decoded Output packets decoded count
 * @param error_count Output error count
 * @return true if successful
 */
bool subghz_receiver_get_stats(
    SubGhzReceiver *receiver, uint32_t *packets_received, uint32_t *packets_decoded, uint32_t *error_count
);

/**
 * @brief Enable/disable protocol filtering
 * @param receiver Receiver instance
 * @param protocol_name Protocol name to filter (NULL for all)
 * @param enabled Enable flag
 * @return true if successful
 */
bool subghz_receiver_set_protocol_filter(SubGhzReceiver *receiver, const char *protocol_name, bool enabled);

#ifdef __cplusplus
}
#endif

#endif // __SUBGHZ_RECEIVER_H__
