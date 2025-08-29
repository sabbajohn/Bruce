#ifndef __SUBGHZ_CONFIG_H__
#define __SUBGHZ_CONFIG_H__

/**
 * @brief SubGHz module configuration
 *
 * Central configuration for SubGHz module features and settings
 */

// Feature flags
#define SUBGHZ_ENABLE_FREQUENCY_ANALYZER 1
#define SUBGHZ_ENABLE_SIGNAL_RECORDER 1
#define SUBGHZ_ENABLE_SIGNAL_PLAYER 1
#define SUBGHZ_ENABLE_PROTOCOL_DECODER 1
#define SUBGHZ_ENABLE_RAW_CAPTURE 1
#define SUBGHZ_ENABLE_ATTACKS 1

// Protocol support
#define SUBGHZ_ENABLE_PRINCETON_PROTOCOL 1
#define SUBGHZ_ENABLE_CAME_PROTOCOL 1   // ✅ Implemented
#define SUBGHZ_ENABLE_KEELOQ_PROTOCOL 1 // ✅ Implemented
#define SUBGHZ_ENABLE_SOMFY_PROTOCOL 1  // ✅ Implemented

// Hardware configurations
#define SUBGHZ_DEFAULT_FREQUENCY 433.92f // MHz
#define SUBGHZ_DEFAULT_PRESET SubGhzPreset2FSKDev476Async
#define SUBGHZ_DEFAULT_TX_POWER 10        // dBm
#define SUBGHZ_DEFAULT_RSSI_THRESHOLD -80 // dBm

// Memory pool sizes (in bytes)
#define SUBGHZ_MEMORY_POOL_PROTOCOL_SIZE (32 * 1024) // 32KB for protocol processing
#define SUBGHZ_MEMORY_POOL_SIGNAL_SIZE (64 * 1024)   // 64KB for signal data
#define SUBGHZ_MEMORY_POOL_STRING_SIZE (16 * 1024)   // 16KB for strings
#define SUBGHZ_MEMORY_POOL_TEMP_SIZE (8 * 1024)      // 8KB for temporary data

// Signal recording limits
#define SUBGHZ_MAX_RECORDING_DURATION_MS 30000 // 30 seconds
#define SUBGHZ_MAX_SIGNAL_SAMPLES 10000        // Maximum samples per signal
#define SUBGHZ_MAX_SAVED_SIGNALS 100           // Maximum saved signals

// Frequency analysis
#define SUBGHZ_FREQUENCY_SCAN_START 300.0f // MHz
#define SUBGHZ_FREQUENCY_SCAN_END 928.0f   // MHz
#define SUBGHZ_FREQUENCY_SCAN_STEP 0.25f   // MHz
#define SUBGHZ_FREQUENCY_SCAN_DWELL_MS 100 // milliseconds per frequency

// Protocol detection
#define SUBGHZ_PROTOCOL_TIMEOUT_MS 5000    // Protocol detection timeout
#define SUBGHZ_MAX_PROTOCOL_NAME_LENGTH 32 // Maximum protocol name length
#define SUBGHZ_MAX_PROTOCOL_DATA_SIZE 256  // Maximum protocol data size

// File system
#define SUBGHZ_SIGNALS_DIR "/subghz"
#define SUBGHZ_SIGNALS_EXTENSION ".sub"
#define SUBGHZ_CONFIG_FILE "/subghz/config.json"

// UI configuration
#define SUBGHZ_UI_UPDATE_INTERVAL_MS 50 // UI update interval
#define SUBGHZ_UI_TIMEOUT_MS 300000     // 5 minutes UI timeout
#define SUBGHZ_UI_GRAPH_POINTS 128      // Number of graph points for visualization

// Attack configurations
#define SUBGHZ_ROLLJAM_BUFFER_SIZE 10       // Number of signals to buffer
#define SUBGHZ_REPLAY_MAX_REPEATS 100       // Maximum replay repeats
#define SUBGHZ_BRUTEFORCE_MAX_ATTEMPTS 1000 // Maximum brute force attempts

// Debug configuration
#define SUBGHZ_DEBUG_ENABLED 1            // Enable debug output
#define SUBGHZ_DEBUG_MEMORY_TRACKING 1    // Enable memory tracking
#define SUBGHZ_DEBUG_PROTOCOL_DETECTION 0 // Debug protocol detection
#define SUBGHZ_DEBUG_SIGNAL_ANALYSIS 0    // Debug signal analysis

// Version information
#define SUBGHZ_VERSION_MAJOR 2
#define SUBGHZ_VERSION_MINOR 0
#define SUBGHZ_VERSION_PATCH 0
#define SUBGHZ_VERSION_STRING "2.0.0"

// Compatibility checks
#if defined(LITE_VERSION) && SUBGHZ_ENABLE_ATTACKS
#warning "SubGHz attacks disabled in LITE_VERSION"
#undef SUBGHZ_ENABLE_ATTACKS
#define SUBGHZ_ENABLE_ATTACKS 0
#endif

#if !defined(HAS_CC1101) && !defined(HAS_INTERNAL_RF)
#warning "No RF hardware detected, SubGHz module may have limited functionality"
#endif

// Validation macros
#define SUBGHZ_VALIDATE_FREQUENCY(f) ((f) >= 300.0f && (f) <= 928.0f)
#define SUBGHZ_VALIDATE_POWER(p) ((p) >= -30 && (p) <= 20)
#define SUBGHZ_VALIDATE_DURATION(d) ((d) > 0 && (d) <= SUBGHZ_MAX_RECORDING_DURATION_MS)

#endif // __SUBGHZ_CONFIG_H__
