#ifndef __SUBGHZ_TRANSMITTER_H__
#define __SUBGHZ_TRANSMITTER_H__

#include "subghz_device.h"
#include "subghz_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SubGHz transmitter implementation
 *
 * Handles signal transmission with timing control and power management
 */

// Transmitter instance
typedef struct SubGhzTransmitter SubGhzTransmitter;

// Transmitter configuration
typedef struct {
    float frequency;
    SubGhzPreset preset;
    int8_t power;
    uint32_t repeat_count;
    uint32_t repeat_delay_ms;
    bool async_mode;
} SubGhzTransmitterConfig;

// Transmission status callback
typedef void (*SubGhzTransmitterCallback)(void *context, SubGhzTransmitterState state, uint32_t progress);

/**
 * @brief Create transmitter instance
 * @param device SubGHz device to use
 * @return Transmitter instance or NULL on failure
 */
SubGhzTransmitter *subghz_transmitter_create(SubGhzDevice *device);

/**
 * @brief Free transmitter instance
 * @param transmitter Transmitter instance
 */
void subghz_transmitter_free(SubGhzTransmitter *transmitter);

/**
 * @brief Configure transmitter
 * @param transmitter Transmitter instance
 * @param config Configuration
 * @return true if successful
 */
bool subghz_transmitter_configure(SubGhzTransmitter *transmitter, const SubGhzTransmitterConfig *config);

/**
 * @brief Set status callback
 * @param transmitter Transmitter instance
 * @param callback Callback function
 * @param context Callback context
 */
void subghz_transmitter_set_callback(
    SubGhzTransmitter *transmitter, SubGhzTransmitterCallback callback, void *context
);

/**
 * @brief Start transmission
 * @param transmitter Transmitter instance
 * @param upload_data Upload data array
 * @param upload_count Upload data count
 * @return true if successful
 */
bool subghz_transmitter_start(SubGhzTransmitter *transmitter, uint32_t *upload_data, size_t upload_count);

/**
 * @brief Stop transmission
 * @param transmitter Transmitter instance
 */
void subghz_transmitter_stop(SubGhzTransmitter *transmitter);

/**
 * @brief Check if transmitter is running
 * @param transmitter Transmitter instance
 * @return true if running
 */
bool subghz_transmitter_is_running(SubGhzTransmitter *transmitter);

/**
 * @brief Get current state
 * @param transmitter Transmitter instance
 * @return Current state
 */
SubGhzTransmitterState subghz_transmitter_get_state(SubGhzTransmitter *transmitter);

/**
 * @brief Get transmission progress (0-100)
 * @param transmitter Transmitter instance
 * @return Progress percentage
 */
uint32_t subghz_transmitter_get_progress(SubGhzTransmitter *transmitter);

/**
 * @brief Process transmitter (call from main loop)
 * @param transmitter Transmitter instance
 */
void subghz_transmitter_process(SubGhzTransmitter *transmitter);

/**
 * @brief Get transmitter statistics
 * @param transmitter Transmitter instance
 * @param tx_count Output transmission count
 * @param error_count Output error count
 * @return true if successful
 */
bool subghz_transmitter_get_stats(SubGhzTransmitter *transmitter, uint32_t *tx_count, uint32_t *error_count);

#ifdef __cplusplus
}
#endif

#endif // __SUBGHZ_TRANSMITTER_H__
