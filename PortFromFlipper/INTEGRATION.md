# Integration Guide - Bruce Framework Architecture

## Overview

This document details how to integrate the SubGHZ module into the Bruce framework architecture, following the established patterns and conventions.

---

## Bruce Framework Architecture

### 1. MenuItemInterface Pattern

Bruce uses a standardized menu system based on the `MenuItemInterface` class:

```cpp
class SubGHZMenu : public MenuItemInterface {
public:
    SubGHZMenu() : MenuItemInterface("SubGHZ") {}

    void optionsMenu(void) override;
    void drawIcon(float scale) override;
    void drawIconImg() override;
    bool getTheme() override { return bruceConfig.theme.subghz; }

private:
    void configMenu(void);
    void protocolsMenu(void);
    void attacksMenu(void);
};
```

### 2. Menu Registration

Add to `src/core/main_menu.h`:

```cpp
#include "menu_items/SubGHZMenu.h"

class MainMenu {
public:
    // ... existing menus
    SubGHZMenu subghzMenu;

private:
    std::vector<MenuItemInterface *> _menuItems;
};
```

Add to `src/core/main_menu.cpp` constructor:

```cpp
MainMenu::MainMenu() {
    _menuItems = {
        // ... existing items
#if !defined(REMOVE_SUBGHZ_MENU)
        &subghzMenu,
#endif
        // ... rest of items
    };
}
```

### 3. Configuration Integration

Add to `src/core/config.h`:

```cpp
typedef struct {
    bool subghz = true;           // SubGHZ menu theme enabled
    // ... existing theme options
} BruceTheme;

typedef struct {
    uint8_t subghzModule = CC1101_SPI_MODULE;   // SubGHZ module type
    uint32_t subghzFreq = 433920000;            // Default frequency
    // ... existing RF options (can reuse)
} BruceConfig;
```

---

## Module Structure

### 1. Directory Organization

```
src/modules/subghz/
├── core/
│   ├── subghz_worker.cpp/h         # Main SubGHZ worker thread
│   ├── subghz_manager.cpp/h        # Protocol manager
│   ├── subghz_memory.cpp/h         # Memory management layer
│   └── subghz_hal.cpp/h            # Hardware abstraction layer
├── protocols/
│   ├── protocol_base.cpp/h         # Base protocol class
│   ├── princeton.cpp/h             # Princeton protocol
│   ├── keeloq.cpp/h               # KeeLoq protocol
│   ├── came.cpp/h                 # CAME protocol
│   └── ...                        # Other protocols
├── attacks/
│   ├── rolling_attacks.cpp/h       # Rolling code attacks
│   ├── jamming.cpp/h              # Jamming functionality
│   └── replay.cpp/h               # Replay attacks
└── utils/
    ├── signal_buffer.cpp/h         # Signal buffering
    ├── timing.cpp/h               # Precise timing functions
    └── encoding.cpp/h             # Encoding/decoding utilities
```

### 2. Header Integration

Create `include/subghz_integration.h`:

```cpp
#ifndef __SUBGHZ_INTEGRATION_H__
#define __SUBGHZ_INTEGRATION_H__

// Enable/disable SubGHZ compilation
#ifndef REMOVE_SUBGHZ_MENU
#define SUBGHZ_ENABLED
#endif

// SubGHZ memory limits for ESP32
#define SUBGHZ_MAX_PROTOCOLS        20      // Maximum loaded protocols
#define SUBGHZ_SIGNAL_BUFFER_SIZE   8192    // Signal buffer size
#define SUBGHZ_MAX_CONCURRENT_OPS   3       // Max concurrent operations

// Hardware definitions
#ifdef CC1101_SPI_MODULE
#define SUBGHZ_USE_CC1101
#define SUBGHZ_CC1101_CS_PIN        bruceConfig.rfModulePins.cs
#define SUBGHZ_CC1101_GDO0_PIN      bruceConfig.rfModulePins.gdo0
#define SUBGHZ_CC1101_GDO2_PIN      bruceConfig.rfModulePins.gdo2
#endif

#include "modules/subghz/core/subghz_manager.h"

#endif
```

---

## Hardware Integration

### 1. CC1101 Driver Integration

Reuse existing Bruce CC1101 infrastructure:

```cpp
// In subghz_hal.cpp
#include "modules/rf/cc1101_driver.h"

class SubGHZHAL {
private:
    static bool cc1101_initialized;

public:
    static bool init() {
        if (!cc1101_initialized) {
            cc1101_initialized = initCC1101();
        }
        return cc1101_initialized;
    }

    static void setFrequency(uint32_t freq) {
        CC1101_setFreq(freq);
    }

    static void setModulation(uint8_t mod) {
        CC1101_setModulation(mod);
    }

    // Reuse Bruce's existing CC1101 functions
    static void transmit(uint8_t* data, size_t len) {
        // Use existing Bruce RF transmission
    }

    static bool receive(uint8_t* buffer, size_t* len) {
        // Use existing Bruce RF reception
    }
};
```

### 2. GPIO Integration

Leverage Bruce's pin configuration system:

```cpp
// Use Bruce's existing RF pin configuration
struct SubGHZPins {
    uint8_t cs_pin = bruceConfig.rfModulePins.cs;
    uint8_t gdo0_pin = bruceConfig.rfModulePins.gdo0;
    uint8_t gdo2_pin = bruceConfig.rfModulePins.gdo2;
    uint8_t sck_pin = bruceConfig.rfModulePins.sck;
    uint8_t mosi_pin = bruceConfig.rfModulePins.mosi;
    uint8_t miso_pin = bruceConfig.rfModulePins.miso;
};
```

---

## Memory Management Strategy

### 1. ESP32 Memory Constraints

```cpp
// Memory pools for efficient allocation
class SubGHZMemoryManager {
private:
    static constexpr size_t PROTOCOL_POOL_SIZE = 32 * 1024;    // 32KB for protocols
    static constexpr size_t SIGNAL_POOL_SIZE = 64 * 1024;      // 64KB for signals
    static constexpr size_t ATTACK_POOL_SIZE = 16 * 1024;      // 16KB for attacks

    void* protocol_pool;
    void* signal_pool;
    void* attack_pool;

public:
    bool initialize() {
        protocol_pool = heap_caps_malloc(PROTOCOL_POOL_SIZE, MALLOC_CAP_8BIT);
        signal_pool = heap_caps_malloc(SIGNAL_POOL_SIZE, MALLOC_CAP_8BIT);
        attack_pool = heap_caps_malloc(ATTACK_POOL_SIZE, MALLOC_CAP_8BIT);

        return protocol_pool && signal_pool && attack_pool;
    }

    void cleanup() {
        if (protocol_pool) free(protocol_pool);
        if (signal_pool) free(signal_pool);
        if (attack_pool) free(attack_pool);
    }
};
```

### 2. Protocol Loading Strategy

```cpp
// Dynamic protocol loading to save memory
class ProtocolManager {
private:
    std::vector<SubGHZProtocol*> loaded_protocols;
    static constexpr size_t MAX_LOADED_PROTOCOLS = 5;

public:
    bool loadProtocol(const char* protocol_name) {
        if (loaded_protocols.size() >= MAX_LOADED_PROTOCOLS) {
            unloadLeastUsedProtocol();
        }

        SubGHZProtocol* protocol = createProtocol(protocol_name);
        if (protocol) {
            loaded_protocols.push_back(protocol);
            return true;
        }
        return false;
    }

    void unloadProtocol(const char* protocol_name) {
        // Remove and cleanup protocol
    }
};
```

---

## Threading Integration

### 1. FreeRTOS Task Management

```cpp
// SubGHZ worker task
class SubGHZWorker {
private:
    TaskHandle_t worker_task;
    QueueHandle_t command_queue;
    bool running = false;

public:
    bool start() {
        command_queue = xQueueCreate(10, sizeof(SubGHZCommand));
        if (!command_queue) return false;

        BaseType_t result = xTaskCreate(
            workerTask,
            "SubGHZ_Worker",
            8192,           // Stack size
            this,
            5,              // Priority
            &worker_task
        );

        return result == pdPASS;
    }

    static void workerTask(void* parameters) {
        SubGHZWorker* worker = (SubGHZWorker*)parameters;
        worker->run();
    }

private:
    void run() {
        SubGHZCommand cmd;
        while (running) {
            if (xQueueReceive(command_queue, &cmd, portMAX_DELAY)) {
                processCommand(cmd);
            }
        }
    }
};
```

---

## UI Integration

### 1. Menu Structure

```cpp
void SubGHZMenu::optionsMenu() {
    options = {
        {"Read",         [=]() { startReceiveMode(); }      },
        {"Send",         [=]() { sendMenu(); }              },
        {"Protocols",    [=]() { protocolsMenu(); }         },
        {"Attacks",      [=]() { attacksMenu(); }           },
        {"File Manager", [=]() { fileManagerMenu(); }       },
        {"Config",       [=]() { configMenu(); }            },
    };

    String title = "SubGHZ";
    if (bruceConfig.subghzModule == CC1101_SPI_MODULE) {
        title += " (CC1101)";
    }

    loopOptions(options, MENU_TYPE_SUBMENU, title.c_str());
}

void SubGHZMenu::attacksMenu() {
    options = {
        {"RollJam",      [=]() { startRollJamAttack(); }    },
        {"RollBack",     [=]() { startRollBackAttack(); }   },
        {"Replay",       [=]() { replayAttackMenu(); }      },
        {"Bruteforce",   [=]() { bruteforceMenu(); }        },
        {"Jamming",      [=]() { jammingMenu(); }           },
        {"Multi-Device", [=]() { multiDeviceMenu(); }       },
        {"Back",         [=]() { optionsMenu(); }           },
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "SubGHZ Attacks");
}
```

### 2. Display Integration

```cpp
// Use Bruce's existing display system
void SubGHZMenu::drawIcon(float scale) {
    clearIconArea();

    // Draw SubGHZ antenna icon
    int centerX = iconCenterX;
    int centerY = iconCenterY;
    int size = 40 * scale;

    // Draw antenna symbol
    tft.drawCircle(centerX, centerY, size/3, bruceConfig.priColor);
    tft.drawLine(centerX, centerY - size/2, centerX, centerY + size/2, bruceConfig.priColor);
    tft.drawLine(centerX - size/3, centerY + size/3, centerX + size/3, centerY + size/3, bruceConfig.priColor);
}
```

---

## File System Integration

### 1. SubGHZ File Format

```cpp
// Use Bruce's file system for SubGHZ files
class SubGHZFileManager {
public:
    static bool saveSignal(const SubGHZSignal& signal, const String& filename) {
        String path = "/subghz/" + filename + ".sub";
        return saveToSD(path, signal.serialize());
    }

    static SubGHZSignal loadSignal(const String& filename) {
        String path = "/subghz/" + filename + ".sub";
        String data = readFromSD(path);
        return SubGHZSignal::deserialize(data);
    }

    static std::vector<String> listSignals() {
        return listFiles("/subghz/", ".sub");
    }
};
```

---

## Configuration System

### 1. Settings Integration

```cpp
// Add SubGHZ settings to Bruce's config system
void addSubGHZSettings() {
    // Frequency settings
    addConfigOption("SubGHZ Frequency", []() {
        return setSubGHZFrequency();
    });

    // Module settings
    addConfigOption("SubGHZ Module", []() {
        return setSubGHZModule();
    });

    // Protocol settings
    addConfigOption("SubGHZ Protocols", []() {
        return configureProtocols();
    });
}
```

---

## Compilation Flags

### 1. Conditional Compilation

Add to `platformio.ini`:

```ini
; SubGHZ Feature Flags
-DSUBGHZ_ENABLED=1
-DSUBGHZ_MAX_PROTOCOLS=20
-DSUBGHZ_USE_CC1101=1

; Memory optimizations for SubGHZ
-DSUBGHZ_OPTIMIZE_MEMORY=1
-DSUBGHZ_DYNAMIC_PROTOCOLS=1

; Attack features
-DSUBGHZ_ENABLE_ATTACKS=1
-DSUBGHZ_ENABLE_ROLLJAM=1
-DSUBGHZ_ENABLE_ROLLBACK=1
```

### 2. Board-Specific Settings

```cpp
// Board-specific SubGHZ configurations
#ifdef BOARD_HAS_CC1101
#define SUBGHZ_DEFAULT_MODULE CC1101_SPI_MODULE
#else
#define SUBGHZ_DEFAULT_MODULE NATIVE_RF_MODULE
#endif

#ifdef ESP32_S3
#define SUBGHZ_MEMORY_OPTIMIZED 1
#define SUBGHZ_MAX_CONCURRENT_SIGNALS 2
#else
#define SUBGHZ_MAX_CONCURRENT_SIGNALS 1
#endif
```

---

## Testing Strategy

### 1. Unit Tests

```cpp
// Create test cases for critical components
namespace SubGHZTests {
    bool testMemoryManager() {
        SubGHZMemoryManager mgr;
        return mgr.initialize() && mgr.runStressTest();
    }

    bool testProtocolDecoding() {
        PrincetonProtocol protocol;
        return protocol.decode(test_signal_data);
    }

    bool testCC1101Integration() {
        return SubGHZHAL::init() && SubGHZHAL::selfTest();
    }
}
```

### 2. Integration Tests

```cpp
// Test full workflow integration
bool testSubGHZWorkflow() {
    // 1. Initialize SubGHZ system
    if (!initializeSubGHZ()) return false;

    // 2. Load a protocol
    if (!loadProtocol("Princeton")) return false;

    // 3. Capture a signal
    SubGHZSignal signal = captureSignal(10000); // 10 second timeout

    // 4. Decode signal
    ProtocolResult result = decodeSignal(signal);

    // 5. Save to file
    return saveSignal(signal, "test_capture");
}
```

---

This integration guide provides the framework for seamlessly integrating SubGHZ functionality into Bruce while maintaining code quality, performance, and the established architectural patterns.
