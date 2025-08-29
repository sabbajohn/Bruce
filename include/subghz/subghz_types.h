#ifndef __SUBGHZ_TYPES_H__
#define __SUBGHZ_TYPES_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SubGHz core types and definitions
 *
 * Adapted from Flipper Zero SubGHz implementation for Bruce framework
 */

// Maximum protocol name length
#define SUBGHZ_PROTOCOL_NAME_SIZE 32

// Maximum key data size
#define SUBGHZ_KEY_SIZE 128

// Maximum RAW data size
#define SUBGHZ_RAW_DATA_SIZE (8 * 1024)

// SubGHz frequency limits (in MHz)
#define SUBGHZ_FREQUENCY_MIN 300.0f
#define SUBGHZ_FREQUENCY_MAX 928.0f

// SubGHz protocol types
typedef enum {
    SubGhzProtocolTypeUnknown = 0,
    SubGhzProtocolTypeStatic,
    SubGhzProtocolTypeDynamic,
    SubGhzProtocolTypeBinRAW,
    SubGhzProtocolTypeMax,
} SubGhzProtocolType;

// SubGHz preset types
typedef enum {
    SubGhzPresetOok270Async,
    SubGhzPresetOok650Async,
    SubGhzPreset2FSKDev238Async,
    SubGhzPreset2FSKDev476Async,
    SubGhzPresetMSK99_97KbAsync,
    SubGhzPresetGFSK9_99KbAsync,
    SubGhzPresetCustom,
    SubGhzPresetMax,
} SubGhzPreset;

#define SUBGHZ_PRESET_COUNT SubGhzPresetMax
#define SUBGHZ_PRESET_FM476 SubGhzPreset2FSKDev476Async

// SubGHz modulation types
typedef enum {
    SubGhzModulationOOK,
    SubGhzModulation2FSK,
    SubGhzModulationGFSK,
    SubGhzModulationMSK,
    SubGhzModulationMax,
} SubGhzModulation;

// SubGHz preset configuration
typedef struct {
    const char *name;
    SubGhzModulation modulation;
    float bandwidth;
    float data_rate;
    float frequency_deviation;
    uint32_t preamble_size;
    uint32_t sync_word;
    bool manchester_enable;
} SubGhzPresetConfig;

// SubGHz decoder status
typedef enum {
    SubGhzDecoderStepReset = 0,
    SubGhzDecoderStepFoundPreambula,
    SubGhzDecoderStepSaveDuration,
    SubGhzDecoderStepCheckDuration,
} SubGhzDecoderStep;

// SubGHz protocol configuration
typedef struct {
    char name[SUBGHZ_PROTOCOL_NAME_SIZE];
    SubGhzProtocolType type;
    uint32_t frequency;
    SubGhzPreset preset;
    uint32_t bit_count;
    uint64_t key;
    uint32_t serial;
    uint8_t btn;
    uint32_t cnt;
    uint32_t seed;
    bool repeat;
    uint8_t data[SUBGHZ_KEY_SIZE];
    size_t data_size;
} SubGhzProtocolConfig;

// SubGHz signal structure
typedef struct {
    uint32_t *durations;
    size_t count;
    size_t capacity;
    bool level;
    uint32_t frequency;
    SubGhzPreset preset;
} SubGhzSignal;

// SubGHz signal data
typedef struct {
    uint32_t *timings;
    size_t timings_count;
    uint32_t frequency;
    SubGhzPreset preset;
    uint32_t te_short;
    uint32_t te_long;
    uint32_t te_delta;
    size_t min_count_bit_for_found;
} SubGhzSignalData;

// SubGHz decoder state
typedef struct {
    const char *protocol_name;
    uint32_t *upload;
    size_t upload_count;
    SubGhzDecoderStep decoder_step;
    uint32_t te_last;
    uint8_t header_count;
    uint8_t btn;
    uint32_t cnt;
    uint32_t serial;
    uint64_t data;
    uint64_t data_count_bit;
    bool decode_data;
    bool decode_count_bit;
} SubGhzDecoderState;

// SubGHz transmitter state
// SubGHz transmitter events
typedef enum {
    SUBGHZ_TRANSMITTER_EVENT_START = 0,
    SUBGHZ_TRANSMITTER_EVENT_COMPLETE,
    SUBGHZ_TRANSMITTER_EVENT_STOP,
    SUBGHZ_TRANSMITTER_EVENT_ERROR
} SubGhzTransmitterEvent;

// SubGHz transmitter state (for compatibility - will be enum in transmitter.h)
typedef enum {
    SUBGHZ_TRANSMITTER_STATE_IDLE = 0,
    SUBGHZ_TRANSMITTER_STATE_TRANSMITTING,
    SUBGHZ_TRANSMITTER_STATE_ERROR
} SubGhzTransmitterState;

// SubGHz receiver state (for compatibility - will be enum in receiver.h)
typedef enum {
    SUBGHZ_RECEIVER_STATE_IDLE = 0,
    SUBGHZ_RECEIVER_STATE_RECEIVING,
    SUBGHZ_RECEIVER_STATE_PROCESSING,
    SUBGHZ_RECEIVER_STATE_ERROR
} SubGhzReceiverState;

// SubGHz environment configuration
typedef struct {
    float frequency_tolerance;
    int32_t rssi_threshold;
    uint32_t detect_raw_gap;
    uint32_t detect_raw_count_threshold;
    bool hopping_enable;
    uint32_t hopping_period;
    uint32_t *hopping_frequencies;
    size_t hopping_frequencies_count;
} SubGhzEnvironment;

// SubGHz key store entry
typedef struct {
    char name[32];
    uint8_t data[74];
    uint8_t type;
} SubGhzKeyStoreEntry;

// SubGHz file format
typedef struct {
    uint32_t version;
    uint32_t frequency;
    char preset[32];
    char protocol[32];
    uint32_t bit;
    uint64_t key;
    uint32_t te;
    uint32_t *raw_data;
    size_t raw_data_count;
} SubGhzFileFormat;

#ifdef __cplusplus
}
#endif

#endif // __SUBGHZ_TYPES_H__
