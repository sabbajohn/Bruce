#ifndef __SUBGHZ_DEVICE_H__
#define __SUBGHZ_DEVICE_H__

#include "subghz_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SubGHz hardware device abstraction
 *
 * Hardware abstraction layer for different SubGHz devices (CC1101, internal RF, etc.)
 */

// Device types
typedef enum {
    SUBGHZ_DEVICE_TYPE_UNKNOWN = 0,
    SUBGHZ_DEVICE_TYPE_CC1101_SPI,
    SUBGHZ_DEVICE_TYPE_INTERNAL_RF,
    SUBGHZ_DEVICE_TYPE_MAX
} SubGhzDeviceType;

// Device state
typedef enum {
    SUBGHZ_DEVICE_STATE_UNKNOWN = 0,
    SUBGHZ_DEVICE_STATE_IDLE,
    SUBGHZ_DEVICE_STATE_RX,
    SUBGHZ_DEVICE_STATE_TX,
    SUBGHZ_DEVICE_STATE_SLEEP,
    SUBGHZ_DEVICE_STATE_ERROR
} SubGhzDeviceState;

// Device capabilities
typedef struct {
    bool supports_tx;
    bool supports_rx;
    bool supports_rssi;
    bool supports_lqi;
    bool supports_crc;
    float min_frequency;
    float max_frequency;
    uint32_t max_packet_size;
    uint8_t supported_presets[SubGhzPresetMax];
} SubGhzDeviceCapabilities;

// Forward declaration
typedef struct SubGhzDevice SubGhzDevice;

// Device interface functions
typedef struct {
    bool (*init)(SubGhzDevice *device);
    bool (*deinit)(SubGhzDevice *device);
    bool (*reset)(SubGhzDevice *device);
    bool (*sleep)(SubGhzDevice *device);
    bool (*idle)(SubGhzDevice *device);
    bool (*set_frequency)(SubGhzDevice *device, float frequency);
    bool (*set_preset)(SubGhzDevice *device, SubGhzPreset preset);
    bool (*set_power)(SubGhzDevice *device, int8_t power);
    bool (*start_rx)(SubGhzDevice *device);
    bool (*stop_rx)(SubGhzDevice *device);
    bool (*start_tx)(SubGhzDevice *device, uint32_t *data, size_t data_size);
    bool (*stop_tx)(SubGhzDevice *device);
    bool (*stop_async)(SubGhzDevice *device);
    bool (*is_frequency_valid)(SubGhzDevice *device, float frequency);
    int32_t (*get_rssi)(SubGhzDevice *device);
    uint8_t (*get_lqi)(SubGhzDevice *device);
    SubGhzDeviceState (*get_state)(SubGhzDevice *device);
    bool (*is_tx_complete)(SubGhzDevice *device);
} SubGhzDeviceInterface;

// Device structure
struct SubGhzDevice {
    SubGhzDeviceType type;
    SubGhzDeviceState state;
    SubGhzDeviceCapabilities capabilities;
    SubGhzDeviceInterface interface;
    void *device_data; // Device-specific data

    // Current configuration
    float current_frequency;
    SubGhzPreset current_preset;
    int8_t current_power;

    // Statistics
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t error_count;
};

/**
 * @brief Create and initialize SubGHz device
 * @param type Device type to create
 * @return Device instance or NULL on failure
 */
SubGhzDevice *subghz_device_create(SubGhzDeviceType type);

/**
 * @brief Free SubGHz device
 * @param device Device instance
 */
void subghz_device_free(SubGhzDevice *device);

/**
 * @brief Initialize SubGHz device
 * @param device Device instance
 * @return true if successful
 */
bool subghz_device_init(SubGhzDevice *device);

/**
 * @brief Deinitialize SubGHz device
 * @param device Device instance
 * @return true if successful
 */
bool subghz_device_deinit(SubGhzDevice *device);

/**
 * @brief Reset SubGHz device
 * @param device Device instance
 * @return true if successful
 */
bool subghz_device_reset(SubGhzDevice *device);

/**
 * @brief Set device to sleep mode
 * @param device Device instance
 * @return true if successful
 */
bool subghz_device_sleep(SubGhzDevice *device);

/**
 * @brief Set device to idle mode
 * @param device Device instance
 * @return true if successful
 */
bool subghz_device_idle(SubGhzDevice *device);

/**
 * @brief Set operating frequency
 * @param device Device instance
 * @param frequency Frequency in MHz
 * @return true if successful
 */
bool subghz_device_set_frequency(SubGhzDevice *device, float frequency);

/**
 * @brief Set modulation preset
 * @param device Device instance
 * @param preset Modulation preset
 * @return true if successful
 */
bool subghz_device_set_preset(SubGhzDevice *device, SubGhzPreset preset);

/**
 * @brief Set transmission power
 * @param device Device instance
 * @param power Power in dBm
 * @return true if successful
 */
bool subghz_device_set_power(SubGhzDevice *device, int8_t power);

/**
 * @brief Start receiver mode
 * @param device Device instance
 * @return true if successful
 */
bool subghz_device_start_rx(SubGhzDevice *device);

/**
 * @brief Start transmitter mode
 * @param device Device instance
 * @param data Data to transmit
 * @param data_size Data size
 * @return true if successful
 */
bool subghz_device_start_tx(SubGhzDevice *device, uint32_t *data, size_t data_size);

/**
 * @brief Stop async operation
 * @param device Device instance
 * @return true if successful
 */
bool subghz_device_stop_async(SubGhzDevice *device);

/**
 * @brief Check if frequency is valid for device
 * @param device Device instance
 * @param frequency Frequency in MHz
 * @return true if valid
 */
bool subghz_device_is_frequency_valid(SubGhzDevice *device, float frequency);

/**
 * @brief Get RSSI value
 * @param device Device instance
 * @return RSSI in dBm
 */
int32_t subghz_device_get_rssi(SubGhzDevice *device);

/**
 * @brief Get LQI value
 * @param device Device instance
 * @return LQI value (0-255)
 */
uint8_t subghz_device_get_lqi(SubGhzDevice *device);

/**
 * @brief Get current device state
 * @param device Device instance
 * @return Current state
 */
SubGhzDeviceState subghz_device_get_state(SubGhzDevice *device);

/**
 * @brief Check if transmission is complete
 * @param device Device instance
 * @return true if transmission is complete
 */
bool subghz_device_is_tx_complete(SubGhzDevice *device);

/**
 * @brief Get device capabilities
 * @param device Device instance
 * @return Device capabilities
 */
SubGhzDeviceCapabilities subghz_device_get_capabilities(SubGhzDevice *device);

/**
 * @brief Get device type name
 * @param type Device type
 * @return Type name string
 */
const char *subghz_device_get_type_name(SubGhzDeviceType type);

/**
 * @brief Get device state name
 * @param state Device state
 * @return State name string
 */
const char *subghz_device_get_state_name(SubGhzDeviceState state);

/**
 * @brief Stop receiver
 * @param device Device instance
 * @return true on success
 */
bool subghz_device_stop_rx(SubGhzDevice *device);

/**
 * @brief Stop transmitter
 * @param device Device instance
 * @return true on success
 */
bool subghz_device_stop_tx(SubGhzDevice *device);

/**
 * @brief Write to device pin (stub for compatibility)
 * @param device Device instance
 * @param state Pin state
 * @return true on success
 */
bool subghz_device_write_pin(SubGhzDevice *device, bool state);

#ifdef __cplusplus
}
#endif

#endif // __SUBGHZ_DEVICE_H__
