#ifndef __SUBGHZ_CORE_H__
#define __SUBGHZ_CORE_H__

#include "subghz_memory.h"
#include "subghz_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SubGHz core functionality
 *
 * Main interface for SubGHz operations in Bruce framework
 */

// SubGHz core instance
typedef struct SubGhzCore SubGhzCore;

/**
 * @brief Initialize SubGHz core system
 * @return Pointer to SubGHz core instance or NULL on failure
 */
SubGhzCore *subghz_core_init(void);

/**
 * @brief Deinitialize SubGHz core system
 * @param core SubGHz core instance
 */
void subghz_core_deinit(SubGhzCore *core);

/**
 * @brief Set frequency for SubGHz operations
 * @param core SubGHz core instance
 * @param frequency Frequency in MHz
 * @return true if successful
 */
bool subghz_core_set_frequency(SubGhzCore *core, float frequency);

/**
 * @brief Get current frequency
 * @param core SubGHz core instance
 * @return Current frequency in MHz
 */
float subghz_core_get_frequency(SubGhzCore *core);

/**
 * @brief Set preset for SubGHz operations
 * @param core SubGHz core instance
 * @param preset Modulation preset
 * @return true if successful
 */
bool subghz_core_set_preset(SubGhzCore *core, SubGhzPreset preset);

/**
 * @brief Get current preset
 * @param core SubGHz core instance
 * @return Current preset
 */
SubGhzPreset subghz_core_get_preset(SubGhzCore *core);

/**
 * @brief Start receiver mode
 * @param core SubGHz core instance
 * @param callback Callback for received signals
 * @param context Context for callback
 * @return true if successful
 */
bool subghz_core_start_rx(
    SubGhzCore *core, void (*callback)(void *context, SubGhzProtocolConfig *config), void *context
);

/**
 * @brief Stop receiver mode
 * @param core SubGHz core instance
 */
void subghz_core_stop_rx(SubGhzCore *core);

/**
 * @brief Start transmitter mode
 * @param core SubGHz core instance
 * @param config Protocol configuration to transmit
 * @param repeat_count Number of repetitions
 * @return true if successful
 */
bool subghz_core_start_tx(SubGhzCore *core, SubGhzProtocolConfig *config, uint32_t repeat_count);

/**
 * @brief Stop transmitter mode
 * @param core SubGHz core instance
 */
void subghz_core_stop_tx(SubGhzCore *core);

/**
 * @brief Check if receiver is running
 * @param core SubGHz core instance
 * @return true if receiver is active
 */
bool subghz_core_is_rx_running(SubGhzCore *core);

/**
 * @brief Check if transmitter is running
 * @param core SubGHz core instance
 * @return true if transmitter is active
 */
bool subghz_core_is_tx_running(SubGhzCore *core);

/**
 * @brief Get RSSI value
 * @param core SubGHz core instance
 * @return RSSI in dBm
 */
int32_t subghz_core_get_rssi(SubGhzCore *core);

/**
 * @brief Set RSSI threshold
 * @param core SubGHz core instance
 * @param threshold RSSI threshold in dBm
 */
void subghz_core_set_rssi_threshold(SubGhzCore *core, int32_t threshold);

/**
 * @brief Load protocol configuration from string
 * @param config Output protocol configuration
 * @param data Protocol data string
 * @return true if successful
 */
bool subghz_core_load_protocol_from_string(SubGhzProtocolConfig *config, const char *data);

/**
 * @brief Save protocol configuration to string
 * @param config Protocol configuration
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return true if successful
 */
bool subghz_core_save_protocol_to_string(SubGhzProtocolConfig *config, char *buffer, size_t buffer_size);

/**
 * @brief Validate frequency
 * @param frequency Frequency in MHz
 * @return true if frequency is valid for SubGHz
 */
bool subghz_core_is_frequency_valid(float frequency);

/**
 * @brief Get preset name
 * @param preset Preset type
 * @return Preset name string
 */
const char *subghz_core_get_preset_name(SubGhzPreset preset);

/**
 * @brief Get protocol type name
 * @param type Protocol type
 * @return Protocol type name string
 */
const char *subghz_core_get_protocol_type_name(SubGhzProtocolType type);

#ifdef __cplusplus
}
#endif

#endif // __SUBGHZ_CORE_H__
