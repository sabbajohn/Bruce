#include "subghz/subghz_device.h"
#include "core/settings.h"
#include "modules/rf/rf_utils.h"
#include "subghz/subghz_memory.h"
#include <esp_log.h>
#include <string.h>

#ifdef USE_CC1101_VIA_SPI
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#endif

static const char *TAG = "SubGhzDevice";

// Device type names
static const char *device_type_names[SUBGHZ_DEVICE_TYPE_MAX] = {"Unknown", "CC1101 SPI", "Internal RF"};

// Device state names
static const char *device_state_names[] = {"Unknown", "Idle", "RX", "TX", "Sleep", "Error"};

// CC1101 device data
typedef struct {
    bool initialized;
    bool spi_shared;
    uint8_t cs_pin;
    uint8_t gdo0_pin;
    uint8_t gdo2_pin;
} SubGhzCC1101Data;

// CC1101 preset configurations
typedef struct {
    uint8_t mdmcfg4;
    uint8_t mdmcfg3;
    uint8_t mdmcfg2;
    uint8_t deviatn;
    uint8_t frend1;
    uint8_t frend0;
} CC1101PresetConfig;

// CC1101 preset table
static const CC1101PresetConfig cc1101_presets[SubGhzPresetMax] = {
    // OOK270Async
    {0x67, 0x38, 0x32, 0x06, 0xB6, 0x10},
    // OOK650Async
    {0x67, 0x38, 0x32, 0x00, 0xB6, 0x10},
    // 2FSKDev238Async
    {0x5B, 0xF8, 0x13, 0x47, 0xB6, 0x10},
    // 2FSKDev476Async
    {0x5B, 0xF8, 0x13, 0x7F, 0xB6, 0x10},
    // MSK99_97KbAsync
    {0x5B, 0xF8, 0x73, 0x47, 0xB6, 0x10},
    // GFSK9_99KbAsync
    {0x5B, 0xF8, 0x13, 0x47, 0xB6, 0x10},
    // Custom
    {0x67, 0x38, 0x32, 0x06, 0xB6, 0x10}
};

// Forward declarations for CC1101 interface
static bool cc1101_init(SubGhzDevice *device);
static bool cc1101_deinit(SubGhzDevice *device);
static bool cc1101_reset(SubGhzDevice *device);
static bool cc1101_sleep(SubGhzDevice *device);
static bool cc1101_idle(SubGhzDevice *device);
static bool cc1101_set_frequency(SubGhzDevice *device, float frequency);
static bool cc1101_set_preset(SubGhzDevice *device, SubGhzPreset preset);
static bool cc1101_set_power(SubGhzDevice *device, int8_t power);
static bool cc1101_start_rx(SubGhzDevice *device);
static bool cc1101_start_tx(SubGhzDevice *device, uint32_t *data, size_t data_size);
static bool cc1101_stop_async(SubGhzDevice *device);
static bool cc1101_is_frequency_valid(SubGhzDevice *device, float frequency);
static int32_t cc1101_get_rssi(SubGhzDevice *device);
static uint8_t cc1101_get_lqi(SubGhzDevice *device);
static SubGhzDeviceState cc1101_get_state(SubGhzDevice *device);
static bool cc1101_is_tx_complete(SubGhzDevice *device);
static bool cc1101_stop_rx(SubGhzDevice *device);
static bool cc1101_stop_tx(SubGhzDevice *device);

// Internal RF interface (placeholder)
static bool internal_rf_init(SubGhzDevice *device);
static bool internal_rf_deinit(SubGhzDevice *device);
static bool internal_rf_reset(SubGhzDevice *device);
static bool internal_rf_sleep(SubGhzDevice *device);
static bool internal_rf_idle(SubGhzDevice *device);
static bool internal_rf_set_frequency(SubGhzDevice *device, float frequency);
static bool internal_rf_set_preset(SubGhzDevice *device, SubGhzPreset preset);
static bool internal_rf_set_power(SubGhzDevice *device, int8_t power);
static bool internal_rf_start_rx(SubGhzDevice *device);
static bool internal_rf_start_tx(SubGhzDevice *device, uint32_t *data, size_t data_size);
static bool internal_rf_stop_async(SubGhzDevice *device);
static bool internal_rf_is_frequency_valid(SubGhzDevice *device, float frequency);
static int32_t internal_rf_get_rssi(SubGhzDevice *device);
static uint8_t internal_rf_get_lqi(SubGhzDevice *device);
static SubGhzDeviceState internal_rf_get_state(SubGhzDevice *device);
static bool internal_rf_is_tx_complete(SubGhzDevice *device);
static bool internal_rf_stop_rx(SubGhzDevice *device);
static bool internal_rf_stop_tx(SubGhzDevice *device);

SubGhzDevice *subghz_device_create(SubGhzDeviceType type) {
    if (type >= SUBGHZ_DEVICE_TYPE_MAX) {
        ESP_LOGE(TAG, "Invalid device type: %d", type);
        return NULL;
    }

    SubGhzDevice *device =
        (SubGhzDevice *)subghz_malloc(SUBGHZ_POOL_PROTOCOL, sizeof(SubGhzDevice), SUBGHZ_MEM_FLAG_ZERO);
    if (!device) {
        ESP_LOGE(TAG, "Failed to allocate device");
        return NULL;
    }

    device->type = type;
    device->state = SUBGHZ_DEVICE_STATE_UNKNOWN;
    device->current_frequency = 433.92f;
    device->current_preset = SubGhzPresetOok650Async;
    device->current_power = 10; // Default 10 dBm

    // Initialize capabilities and interface based on device type
    switch (type) {
        case SUBGHZ_DEVICE_TYPE_CC1101_SPI:
            // CC1101 capabilities
            device->capabilities.supports_tx = true;
            device->capabilities.supports_rx = true;
            device->capabilities.supports_rssi = true;
            device->capabilities.supports_lqi = true;
            device->capabilities.supports_crc = true;
            device->capabilities.min_frequency = 300.0f;
            device->capabilities.max_frequency = 928.0f;
            device->capabilities.max_packet_size = 255;

            // Mark all presets as supported for CC1101
            for (int i = 0; i < SubGhzPresetMax; i++) { device->capabilities.supported_presets[i] = 1; }

            // CC1101 interface
            device->interface.init = cc1101_init;
            device->interface.deinit = cc1101_deinit;
            device->interface.reset = cc1101_reset;
            device->interface.sleep = cc1101_sleep;
            device->interface.idle = cc1101_idle;
            device->interface.set_frequency = cc1101_set_frequency;
            device->interface.set_preset = cc1101_set_preset;
            device->interface.set_power = cc1101_set_power;
            device->interface.start_rx = cc1101_start_rx;
            device->interface.stop_rx = cc1101_stop_rx;
            device->interface.start_tx = cc1101_start_tx;
            device->interface.stop_tx = cc1101_stop_tx;
            device->interface.stop_async = cc1101_stop_async;
            device->interface.is_frequency_valid = cc1101_is_frequency_valid;
            device->interface.get_rssi = cc1101_get_rssi;
            device->interface.get_lqi = cc1101_get_lqi;
            device->interface.get_state = cc1101_get_state;
            device->interface.is_tx_complete = cc1101_is_tx_complete;

            // Allocate CC1101 specific data
            device->device_data =
                subghz_malloc(SUBGHZ_POOL_PROTOCOL, sizeof(SubGhzCC1101Data), SUBGHZ_MEM_FLAG_ZERO);
            if (!device->device_data) {
                ESP_LOGE(TAG, "Failed to allocate CC1101 data");
                subghz_free(SUBGHZ_POOL_PROTOCOL, device);
                return NULL;
            }
            break;

        case SUBGHZ_DEVICE_TYPE_INTERNAL_RF:
            // Internal RF capabilities (limited)
            device->capabilities.supports_tx = true;
            device->capabilities.supports_rx = true;
            device->capabilities.supports_rssi = false;
            device->capabilities.supports_lqi = false;
            device->capabilities.supports_crc = false;
            device->capabilities.min_frequency = 433.0f;
            device->capabilities.max_frequency = 434.0f;
            device->capabilities.max_packet_size = 64;

            // Only basic OOK supported for internal RF
            device->capabilities.supported_presets[SubGhzPresetOok650Async] = 1;

            // Internal RF interface
            device->interface.init = internal_rf_init;
            device->interface.deinit = internal_rf_deinit;
            device->interface.reset = internal_rf_reset;
            device->interface.sleep = internal_rf_sleep;
            device->interface.idle = internal_rf_idle;
            device->interface.set_frequency = internal_rf_set_frequency;
            device->interface.set_preset = internal_rf_set_preset;
            device->interface.set_power = internal_rf_set_power;
            device->interface.start_rx = internal_rf_start_rx;
            device->interface.stop_rx = internal_rf_stop_rx;
            device->interface.start_tx = internal_rf_start_tx;
            device->interface.stop_tx = internal_rf_stop_tx;
            device->interface.stop_async = internal_rf_stop_async;
            device->interface.is_frequency_valid = internal_rf_is_frequency_valid;
            device->interface.get_rssi = internal_rf_get_rssi;
            device->interface.get_lqi = internal_rf_get_lqi;
            device->interface.get_state = internal_rf_get_state;
            device->interface.is_tx_complete = internal_rf_is_tx_complete;
            break;

        default:
            ESP_LOGE(TAG, "Unsupported device type: %d", type);
            subghz_free(SUBGHZ_POOL_PROTOCOL, device);
            return NULL;
    }

    ESP_LOGI(TAG, "Created %s device", device_type_names[type]);
    return device;
}

void subghz_device_free(SubGhzDevice *device) {
    if (!device) return;

    // Deinitialize device
    if (device->interface.deinit) { device->interface.deinit(device); }

    // Free device-specific data
    if (device->device_data) { subghz_free(SUBGHZ_POOL_PROTOCOL, device->device_data); }

    // Free device structure
    subghz_free(SUBGHZ_POOL_PROTOCOL, device);
    ESP_LOGI(TAG, "Device freed");
}

// Wrapper functions
bool subghz_device_init(SubGhzDevice *device) {
    if (!device || !device->interface.init) return false;
    return device->interface.init(device);
}

bool subghz_device_deinit(SubGhzDevice *device) {
    if (!device || !device->interface.deinit) return false;
    return device->interface.deinit(device);
}

bool subghz_device_reset(SubGhzDevice *device) {
    if (!device || !device->interface.reset) return false;
    return device->interface.reset(device);
}

bool subghz_device_sleep(SubGhzDevice *device) {
    if (!device || !device->interface.sleep) return false;
    return device->interface.sleep(device);
}

bool subghz_device_idle(SubGhzDevice *device) {
    if (!device || !device->interface.idle) return false;
    return device->interface.idle(device);
}

bool subghz_device_set_frequency(SubGhzDevice *device, float frequency) {
    if (!device || !device->interface.set_frequency) return false;
    bool result = device->interface.set_frequency(device, frequency);
    if (result) { device->current_frequency = frequency; }
    return result;
}

bool subghz_device_set_preset(SubGhzDevice *device, SubGhzPreset preset) {
    if (!device || !device->interface.set_preset) return false;
    if (preset >= SubGhzPresetMax || !device->capabilities.supported_presets[preset]) {
        ESP_LOGE(TAG, "Preset %d not supported by device", preset);
        return false;
    }
    bool result = device->interface.set_preset(device, preset);
    if (result) { device->current_preset = preset; }
    return result;
}

bool subghz_device_set_power(SubGhzDevice *device, int8_t power) {
    if (!device || !device->interface.set_power) return false;
    bool result = device->interface.set_power(device, power);
    if (result) { device->current_power = power; }
    return result;
}

bool subghz_device_start_rx(SubGhzDevice *device) {
    if (!device || !device->interface.start_rx) return false;
    if (!device->capabilities.supports_rx) {
        ESP_LOGE(TAG, "Device does not support RX");
        return false;
    }
    bool result = device->interface.start_rx(device);
    if (result) {
        device->state = SUBGHZ_DEVICE_STATE_RX;
        device->rx_count++;
    }
    return result;
}

bool subghz_device_start_tx(SubGhzDevice *device, uint32_t *data, size_t data_size) {
    if (!device || !device->interface.start_tx) return false;
    if (!device->capabilities.supports_tx) {
        ESP_LOGE(TAG, "Device does not support TX");
        return false;
    }
    if (data_size > device->capabilities.max_packet_size) {
        ESP_LOGE(
            TAG, "Data size %zu exceeds max packet size %u", data_size, device->capabilities.max_packet_size
        );
        return false;
    }
    bool result = device->interface.start_tx(device, data, data_size);
    if (result) {
        device->state = SUBGHZ_DEVICE_STATE_TX;
        device->tx_count++;
    }
    return result;
}

bool subghz_device_stop_async(SubGhzDevice *device) {
    if (!device || !device->interface.stop_async) return false;
    bool result = device->interface.stop_async(device);
    if (result) { device->state = SUBGHZ_DEVICE_STATE_IDLE; }
    return result;
}

bool subghz_device_is_frequency_valid(SubGhzDevice *device, float frequency) {
    if (!device || !device->interface.is_frequency_valid) return false;
    return device->interface.is_frequency_valid(device, frequency);
}

int32_t subghz_device_get_rssi(SubGhzDevice *device) {
    if (!device || !device->interface.get_rssi || !device->capabilities.supports_rssi) return -100;
    return device->interface.get_rssi(device);
}

uint8_t subghz_device_get_lqi(SubGhzDevice *device) {
    if (!device || !device->interface.get_lqi || !device->capabilities.supports_lqi) return 0;
    return device->interface.get_lqi(device);
}

SubGhzDeviceState subghz_device_get_state(SubGhzDevice *device) {
    if (!device || !device->interface.get_state) return SUBGHZ_DEVICE_STATE_UNKNOWN;
    return device->interface.get_state(device);
}

bool subghz_device_is_tx_complete(SubGhzDevice *device) {
    if (!device || !device->interface.is_tx_complete) return true;
    return device->interface.is_tx_complete(device);
}

SubGhzDeviceCapabilities subghz_device_get_capabilities(SubGhzDevice *device) {
    if (!device) {
        SubGhzDeviceCapabilities empty = {0};
        return empty;
    }
    return device->capabilities;
}

const char *subghz_device_get_type_name(SubGhzDeviceType type) {
    if (type >= SUBGHZ_DEVICE_TYPE_MAX) return "Unknown";
    return device_type_names[type];
}

const char *subghz_device_get_state_name(SubGhzDeviceState state) {
    if (state > SUBGHZ_DEVICE_STATE_ERROR) return "Unknown";
    return device_state_names[state];
}

// CC1101 Implementation Functions

static bool cc1101_init(SubGhzDevice *device) {
    if (!device || !device->device_data) return false;

    SubGhzCC1101Data *cc1101_data = (SubGhzCC1101Data *)device->device_data;

    ESP_LOGI(TAG, "Initializing CC1101 device");

#ifdef USE_CC1101_VIA_SPI
    // Check if CC1101 module is configured in Bruce
    if (bruceConfig.rfModule != CC1101_SPI_MODULE) {
        ESP_LOGE(TAG, "CC1101 not configured in Bruce settings");
        return false;
    }

    // Initialize CC1101 using Bruce's RF system
    if (!initRfModule("", device->current_frequency)) {
        ESP_LOGE(TAG, "Failed to initialize CC1101 via Bruce RF system");
        return false;
    }

    // Check CC1101 connection
    if (!ELECHOUSE_cc1101.getCC1101()) {
        ESP_LOGE(TAG, "CC1101 connection check failed");
        return false;
    }

    cc1101_data->initialized = true;
    device->state = SUBGHZ_DEVICE_STATE_IDLE;

    ESP_LOGI(TAG, "CC1101 initialized successfully");
    return true;
#else
    ESP_LOGE(TAG, "CC1101 support not compiled in");
    return false;
#endif
}

static bool cc1101_deinit(SubGhzDevice *device) {
    if (!device || !device->device_data) return false;

    SubGhzCC1101Data *cc1101_data = (SubGhzCC1101Data *)device->device_data;

    if (cc1101_data->initialized) {
        deinitRfModule();
        cc1101_data->initialized = false;
        device->state = SUBGHZ_DEVICE_STATE_UNKNOWN;
        ESP_LOGI(TAG, "CC1101 deinitialized");
    }

    return true;
}

static bool cc1101_reset(SubGhzDevice *device) {
    if (!device) return false;

#ifdef USE_CC1101_VIA_SPI
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.Init();
    device->state = SUBGHZ_DEVICE_STATE_IDLE;
    ESP_LOGI(TAG, "CC1101 reset");
    return true;
#else
    return false;
#endif
}

static bool cc1101_sleep(SubGhzDevice *device) {
    if (!device) return false;

#ifdef USE_CC1101_VIA_SPI
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.goSleep();
    device->state = SUBGHZ_DEVICE_STATE_SLEEP;
    return true;
#else
    return false;
#endif
}

static bool cc1101_idle(SubGhzDevice *device) {
    if (!device) return false;

#ifdef USE_CC1101_VIA_SPI
    ELECHOUSE_cc1101.setSidle();
    device->state = SUBGHZ_DEVICE_STATE_IDLE;
    return true;
#else
    return false;
#endif
}

static bool cc1101_set_frequency(SubGhzDevice *device, float frequency) {
    if (!device) return false;

#ifdef USE_CC1101_VIA_SPI
    if (!cc1101_is_frequency_valid(device, frequency)) {
        ESP_LOGE(TAG, "Invalid frequency for CC1101: %.2f MHz", frequency);
        return false;
    }

    ELECHOUSE_cc1101.setSidle();
    setMHZ(frequency);
    ESP_LOGI(TAG, "CC1101 frequency set to %.2f MHz", frequency);
    return true;
#else
    return false;
#endif
}

static bool cc1101_set_preset(SubGhzDevice *device, SubGhzPreset preset) {
    if (!device || preset >= SubGhzPresetMax) return false;

#ifdef USE_CC1101_VIA_SPI
    const CC1101PresetConfig *config = &cc1101_presets[preset];

    ELECHOUSE_cc1101.setSidle();

    // Apply preset configuration
    ELECHOUSE_cc1101.SpiWriteReg(0x10, config->mdmcfg4); // MDMCFG4
    ELECHOUSE_cc1101.SpiWriteReg(0x11, config->mdmcfg3); // MDMCFG3
    ELECHOUSE_cc1101.SpiWriteReg(0x12, config->mdmcfg2); // MDMCFG2
    ELECHOUSE_cc1101.SpiWriteReg(0x15, config->deviatn); // DEVIATN
    ELECHOUSE_cc1101.SpiWriteReg(0x21, config->frend1);  // FREND1
    ELECHOUSE_cc1101.SpiWriteReg(0x22, config->frend0);  // FREND0

    // Set packet format for SubGHz
    ELECHOUSE_cc1101.setPktFormat(3); // Asynchronous serial mode

    ESP_LOGI(TAG, "CC1101 preset set to %d", preset);
    return true;
#else
    return false;
#endif
}

static bool cc1101_set_power(SubGhzDevice *device, int8_t power) {
    if (!device) return false;

#ifdef USE_CC1101_VIA_SPI
    // Clamp power to valid range for CC1101
    if (power > 12) power = 12;
    if (power < -30) power = -30;

    ELECHOUSE_cc1101.setPA(power);
    ESP_LOGI(TAG, "CC1101 power set to %d dBm", power);
    return true;
#else
    return false;
#endif
}

static bool cc1101_start_rx(SubGhzDevice *device) {
    if (!device) return false;

#ifdef USE_CC1101_VIA_SPI
    ELECHOUSE_cc1101.SetRx();
    device->state = SUBGHZ_DEVICE_STATE_RX;
    ESP_LOGI(TAG, "CC1101 started RX");
    return true;
#else
    return false;
#endif
}

static bool cc1101_start_tx(SubGhzDevice *device, uint32_t *data, size_t data_size) {
    if (!device || !data) return false;

#ifdef USE_CC1101_VIA_SPI
    ELECHOUSE_cc1101.SetTx();
    device->state = SUBGHZ_DEVICE_STATE_TX;
    ESP_LOGI(TAG, "CC1101 started TX with %zu bytes", data_size);
    return true;
#else
    return false;
#endif
}

static bool cc1101_stop_async(SubGhzDevice *device) {
    if (!device) return false;

#ifdef USE_CC1101_VIA_SPI
    ELECHOUSE_cc1101.setSidle();
    device->state = SUBGHZ_DEVICE_STATE_IDLE;
    ESP_LOGI(TAG, "CC1101 stopped async operation");
    return true;
#else
    return false;
#endif
}

static bool cc1101_is_frequency_valid(SubGhzDevice *device, float frequency) {
    if (!device) return false;

    // CC1101 supports multiple bands
    return (
        (frequency >= 300.0f && frequency <= 348.0f) || // 315 MHz band
        (frequency >= 387.0f && frequency <= 464.0f) || // 433 MHz band
        (frequency >= 779.0f && frequency <= 928.0f)
    ); // 868/915 MHz band
}

static int32_t cc1101_get_rssi(SubGhzDevice *device) {
    if (!device) return -100;

#ifdef USE_CC1101_VIA_SPI
    return ELECHOUSE_cc1101.getRssi();
#else
    return -100;
#endif
}

static uint8_t cc1101_get_lqi(SubGhzDevice *device) {
    if (!device) return 0;

#ifdef USE_CC1101_VIA_SPI
    return ELECHOUSE_cc1101.getLqi();
#else
    return 0;
#endif
}

static SubGhzDeviceState cc1101_get_state(SubGhzDevice *device) {
    if (!device) return SUBGHZ_DEVICE_STATE_UNKNOWN;
    return device->state;
}

static bool cc1101_is_tx_complete(SubGhzDevice *device) {
    if (!device) return true;

#ifdef USE_CC1101_VIA_SPI
    // Check if CC1101 is still in TX mode
    uint8_t state = ELECHOUSE_cc1101.SpiReadStatus(0x35); // MARCSTATE
    return (state != 0x13);                               // 0x13 = TX state
#else
    return true;
#endif
}

// Internal RF Implementation Functions (placeholders)

static bool internal_rf_init(SubGhzDevice *device) {
    if (!device) return false;

    ESP_LOGI(TAG, "Initializing internal RF device");

    // Use Bruce's basic RF initialization
    if (!initRfModule("", device->current_frequency)) {
        ESP_LOGE(TAG, "Failed to initialize internal RF");
        return false;
    }

    device->state = SUBGHZ_DEVICE_STATE_IDLE;
    ESP_LOGI(TAG, "Internal RF initialized");
    return true;
}

static bool internal_rf_deinit(SubGhzDevice *device) {
    if (!device) return false;

    deinitRfModule();
    device->state = SUBGHZ_DEVICE_STATE_UNKNOWN;
    ESP_LOGI(TAG, "Internal RF deinitialized");
    return true;
}

static bool internal_rf_reset(SubGhzDevice *device) {
    if (!device) return false;
    device->state = SUBGHZ_DEVICE_STATE_IDLE;
    return true;
}

static bool internal_rf_sleep(SubGhzDevice *device) {
    if (!device) return false;
    device->state = SUBGHZ_DEVICE_STATE_SLEEP;
    return true;
}

static bool internal_rf_idle(SubGhzDevice *device) {
    if (!device) return false;
    device->state = SUBGHZ_DEVICE_STATE_IDLE;
    return true;
}

static bool internal_rf_set_frequency(SubGhzDevice *device, float frequency) {
    if (!device) return false;

    // Internal RF has limited frequency support
    if (frequency < 433.0f || frequency > 434.0f) {
        ESP_LOGW(TAG, "Internal RF frequency limited to 433-434 MHz, got %.2f", frequency);
        return false;
    }

    bruceConfig.setRfFreq(frequency, 2);
    ESP_LOGI(TAG, "Internal RF frequency set to %.2f MHz", frequency);
    return true;
}

static bool internal_rf_set_preset(SubGhzDevice *device, SubGhzPreset preset) {
    if (!device) return false;

    // Internal RF only supports basic OOK
    if (preset != SubGhzPresetOok650Async) {
        ESP_LOGW(TAG, "Internal RF only supports OOK650Async preset");
        return false;
    }

    return true;
}

static bool internal_rf_set_power(SubGhzDevice *device, int8_t power) {
    if (!device) return false;

    // Internal RF has no power control
    ESP_LOGW(TAG, "Internal RF has no power control");
    return true;
}

static bool internal_rf_start_rx(SubGhzDevice *device) {
    if (!device) return false;

    device->state = SUBGHZ_DEVICE_STATE_RX;
    ESP_LOGI(TAG, "Internal RF started RX");
    return true;
}

static bool internal_rf_start_tx(SubGhzDevice *device, uint32_t *data, size_t data_size) {
    if (!device) return false;

    device->state = SUBGHZ_DEVICE_STATE_TX;
    ESP_LOGI(TAG, "Internal RF started TX");
    return true;
}

static bool internal_rf_stop_async(SubGhzDevice *device) {
    if (!device) return false;

    device->state = SUBGHZ_DEVICE_STATE_IDLE;
    ESP_LOGI(TAG, "Internal RF stopped");
    return true;
}

static bool internal_rf_is_frequency_valid(SubGhzDevice *device, float frequency) {
    if (!device) return false;
    return (frequency >= 433.0f && frequency <= 434.0f);
}

static int32_t internal_rf_get_rssi(SubGhzDevice *device) {
    if (!device) return -100;
    return -60; // Placeholder value
}

static uint8_t internal_rf_get_lqi(SubGhzDevice *device) {
    if (!device) return 0;
    return 0; // Not supported
}

static SubGhzDeviceState internal_rf_get_state(SubGhzDevice *device) {
    if (!device) return SUBGHZ_DEVICE_STATE_UNKNOWN;
    return device->state;
}

static bool internal_rf_is_tx_complete(SubGhzDevice *device) {
    if (!device) return true;
    return true; // Always complete for basic RF
}

// CC1101 stop functions
static bool cc1101_stop_rx(SubGhzDevice *device) {
    if (!device) return false;

    device->state = SUBGHZ_DEVICE_STATE_IDLE;
    ESP_LOGD(TAG, "CC1101 stopped RX");
    return true;
}

static bool cc1101_stop_tx(SubGhzDevice *device) {
    if (!device) return false;

    device->state = SUBGHZ_DEVICE_STATE_IDLE;
    ESP_LOGD(TAG, "CC1101 stopped TX");
    return true;
}

// Internal RF stop functions
static bool internal_rf_stop_rx(SubGhzDevice *device) {
    if (!device) return false;

    device->state = SUBGHZ_DEVICE_STATE_IDLE;
    ESP_LOGD(TAG, "Internal RF stopped RX");
    return true;
}

static bool internal_rf_stop_tx(SubGhzDevice *device) {
    if (!device) return false;

    device->state = SUBGHZ_DEVICE_STATE_IDLE;
    ESP_LOGD(TAG, "Internal RF stopped TX");
    return true;
}

// Additional functions needed by transmitter/receiver
bool subghz_device_stop_rx(SubGhzDevice *device) {
    if (!device || !device->interface.stop_rx) { return false; }

    bool result = device->interface.stop_rx(device);
    if (result) {
        device->state = SUBGHZ_DEVICE_STATE_IDLE;
        ESP_LOGD(TAG, "Stopped RX");
    }

    return result;
}

bool subghz_device_stop_tx(SubGhzDevice *device) {
    if (!device || !device->interface.stop_tx) { return false; }

    bool result = device->interface.stop_tx(device);
    if (result) {
        device->state = SUBGHZ_DEVICE_STATE_IDLE;
        ESP_LOGD(TAG, "Stopped TX");
    }

    return result;
}

bool subghz_device_write_pin(SubGhzDevice *device, bool state) {
    if (!device) { return false; }

    // For now, this is a placeholder - actual implementation would depend on device type
    // CC1101 might toggle a GPIO pin, internal RF might change PA enable, etc.
    ESP_LOGD(TAG, "Write pin state: %s", state ? "HIGH" : "LOW");

    return true;
}
