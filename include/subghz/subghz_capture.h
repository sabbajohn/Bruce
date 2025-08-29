#pragma once
#include "subghz/subghz_core.h"
#include "subghz/subghz_receiver.h"
#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SubGHz Capture Integration for Bruce
 *
 * This module bridges the SubGHz system with the Bruce's RF capture capabilities
 * providing practical signal capture and analysis using CC1101
 */

// Capture configuration
typedef struct {
    float frequency;              // Capture frequency in MHz
    uint32_t timeout_ms;          // Capture timeout
    bool raw_capture_enabled;     // Enable raw signal capture
    bool protocol_decode_enabled; // Enable protocol decoding
    int8_t rssi_threshold;        // RSSI threshold for signal detection
    size_t max_signals;           // Maximum signals to capture
} SubGhzCaptureConfig;

// Capture result
typedef struct {
    uint32_t signals_captured;    // Number of signals captured
    uint32_t protocols_detected;  // Number of protocols detected
    float average_rssi;           // Average RSSI during capture
    uint32_t capture_duration_ms; // Actual capture duration
    bool timeout_reached;         // If capture ended due to timeout
} SubGhzCaptureResult;

/**
 * @brief Initialize SubGHz capture system
 * @return true if successful
 */
bool subghz_capture_init(void);

/**
 * @brief Deinitialize SubGHz capture system
 */
void subghz_capture_deinit(void);

/**
 * @brief Start practical signal capture using CC1101
 * @param config Capture configuration
 * @param result Output capture results
 * @return true if capture completed successfully
 */
bool subghz_capture_start(const SubGhzCaptureConfig *config, SubGhzCaptureResult *result);

/**
 * @brief Stop ongoing capture
 */
void subghz_capture_stop(void);

/**
 * @brief Check if capture is currently running
 * @return true if capture is active
 */
bool subghz_capture_is_running(void);

/**
 * @brief Get current capture statistics
 * @param result Output current statistics
 * @return true if statistics are valid
 */
bool subghz_capture_get_stats(SubGhzCaptureResult *result);

/**
 * @brief Perform frequency scan to find active signals
 * @param start_freq Start frequency in MHz
 * @param end_freq End frequency in MHz
 * @param step_freq Step frequency in MHz
 * @param scan_time_ms Time to scan each frequency
 * @return Best frequency found (0 if none)
 */
float subghz_capture_frequency_scan(float start_freq, float end_freq, float step_freq, uint32_t scan_time_ms);

#ifdef __cplusplus
}
#endif
