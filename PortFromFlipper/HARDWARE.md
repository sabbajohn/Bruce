# Hardware Compatibility Matrix

## Overview

This document outlines hardware compatibility and configuration requirements for SubGHZ integration across different Bruce-supported devices.

---

## Supported Devices

### ✅ Fully Compatible (CC1101 Required)

#### 1. **LilyGo T-Embed CC1101**

- **Board**: `lilygo-t-embed-cc1101`
- **CC1101**: Integrated on-board
- **Memory**: ESP32-S3 with adequate RAM
- **Status**: **Primary Target**
- **Pins**:
  - CS: GPIO 7
  - GDO0: GPIO 9
  - GDO2: GPIO 8
  - SCK: GPIO 12
  - MOSI: GPIO 13
  - MISO: GPIO 11

#### 2. **Devices with External CC1101**

- **Boards**: All ESP32/ESP32-S3 Bruce devices
- **CC1101**: External module required
- **Connection**: SPI interface
- **Status**: **Compatible with wiring**

---

## CC1101 Module Requirements

### 1. **CC1101 Specifications**

- **Frequency Range**: 300-348MHz, 387-464MHz, 779-928MHz
- **Modulation**: ASK, 2-FSK, 4-FSK, GFSK, MSK, OOK
- **Data Rate**: 0.6 to 500 kBaud
- **Power**: 3.3V operation
- **Interface**: SPI (4-wire + GDO pins)

### 2. **Required Connections**

```
CC1101 Module    ESP32 GPIO
-----------      ----------
VCC       →      3.3V
GND       →      GND
SCK       →      Configurable (default: varies by board)
MOSI      →      Configurable (default: varies by board)
MISO      →      Configurable (default: varies by board)
CS        →      Configurable (default: varies by board)
GDO0      →      Configurable (interrupt pin)
GDO2      →      Configurable (optional)
```

---

## Board-Specific Configurations

### 1. **LilyGo T-Embed CC1101**

```cpp
// Built-in CC1101 configuration
#define CC1101_CS_PIN       7
#define CC1101_GDO0_PIN     9
#define CC1101_GDO2_PIN     8
#define CC1101_SCK_PIN      12
#define CC1101_MOSI_PIN     13
#define CC1101_MISO_PIN     11

// Memory allocation
#define SUBGHZ_SIGNAL_BUFFER_SIZE   8192    // 8KB signal buffer
#define SUBGHZ_MAX_PROTOCOLS        15      // Up to 15 protocols
#define SUBGHZ_ENABLE_ALL_ATTACKS   1       // Full attack suite
```

**Advantages**:

- ✅ Built-in CC1101 (no external wiring)
- ✅ ESP32-S3 with sufficient memory
- ✅ Dedicated RF antenna
- ✅ Optimal performance

**Limitations**:

- ⚠️ Single board support initially

### 2. **ESP32-S3 Devices (External CC1101)**

```cpp
// External CC1101 configuration template
#define CC1101_CS_PIN       bruceConfig.cc1101Pins.cs    // User configurable
#define CC1101_GDO0_PIN     bruceConfig.cc1101Pins.gdo0  // User configurable
#define CC1101_GDO2_PIN     bruceConfig.cc1101Pins.gdo2  // User configurable
#define CC1101_SCK_PIN      bruceConfig.spiPins.sck      // Shared SPI
#define CC1101_MOSI_PIN     bruceConfig.spiPins.mosi     // Shared SPI
#define CC1101_MISO_PIN     bruceConfig.spiPins.miso     // Shared SPI

// Reduced memory allocation
#define SUBGHZ_SIGNAL_BUFFER_SIZE   4096    // 4KB signal buffer
#define SUBGHZ_MAX_PROTOCOLS        10      // Up to 10 protocols
#define SUBGHZ_BASIC_ATTACKS_ONLY   1       // Basic attacks only
```

**Examples**:

- LilyGo T-Display S3
- LilyGo T-HMI
- M5Stack CoreS3
- M5Stack Cardputer

**Advantages**:

- ✅ Sufficient memory for core functionality
- ✅ Flexible pin configuration
- ✅ Cost-effective solution

**Limitations**:

- ⚠️ Requires external CC1101 module
- ⚠️ Additional wiring needed
- ⚠️ Reduced protocol capacity

### 3. **ESP32 Classic Devices (Limited Support)**

```cpp
// ESP32 classic with memory constraints
#define SUBGHZ_SIGNAL_BUFFER_SIZE   2048    // 2KB signal buffer
#define SUBGHZ_MAX_PROTOCOLS        5       // Up to 5 protocols
#define SUBGHZ_MINIMAL_MODE         1       // Minimal features only
#define SUBGHZ_NO_CONCURRENT_OPS    1       // Single operation only
```

**Examples**:

- ESP32-DevKit
- M5Stack Core/Core2

**Advantages**:

- ✅ Basic SubGHZ functionality
- ✅ Legacy device support

**Limitations**:

- ⚠️ Very limited memory
- ⚠️ Reduced protocol support
- ⚠️ No complex attacks
- ⚠️ Single operation mode

---

## Memory Requirements by Feature

### 1. **Core SubGHZ System**

```
Component                    Memory Usage
-----------                  ------------
Base System                  ~15KB RAM
Protocol Manager             ~8KB RAM
CC1101 Driver               ~5KB RAM
Signal Buffers              4-8KB RAM (configurable)
TOTAL CORE:                 ~32-36KB RAM
```

### 2. **Protocol Support**

```
Protocol Type               Memory per Protocol
-------------               -------------------
Static (Princeton/CAME)     ~1.5KB RAM
Dynamic (KeeLoq)            ~3KB RAM
Complex (SecPlus v2)        ~4KB RAM
Maximum Recommended:        10-15 protocols simultaneously
```

### 3. **Attack Features**

```
Attack Type                 Memory Usage
-----------                 ------------
Basic Replay                ~2KB RAM
RollJam                     ~5KB RAM
RollBack                    ~4KB RAM
Bruteforce                  ~3KB RAM
Multi-Device Coordination   ~6KB RAM
TOTAL ATTACKS:              ~20KB RAM
```

### 4. **Total Memory Budget**

```
Configuration               Total RAM Usage
-------------               ---------------
Minimal (ESP32)             ~40KB RAM
Standard (ESP32-S3)         ~60KB RAM
Full Featured (T-Embed)     ~80KB RAM
```

---

## Performance Characteristics

### 1. **Signal Processing Performance**

| Device Type | Decode Speed | Max Protocols | Concurrent Ops |
| ----------- | ------------ | ------------- | -------------- |
| ESP32-S3    | ~1ms/signal  | 15            | 3              |
| ESP32       | ~3ms/signal  | 5             | 1              |

### 2. **Attack Performance**

| Attack Type | ESP32-S3 | ESP32   | Notes            |
| ----------- | -------- | ------- | ---------------- |
| RollJam     | Full     | Basic   | Timing critical  |
| RollBack    | Full     | Limited | Memory intensive |
| Replay      | Full     | Full    | Lightweight      |
| Bruteforce  | Full     | Basic   | CPU intensive    |

---

## Hardware Limitations

### 1. **ESP32 Classic Limitations**

- ❌ **Memory Constraint**: ~200KB available heap
- ❌ **Single Core**: Limited multitasking
- ❌ **No PSRAM**: Cannot expand memory
- ⚠️ **Basic Support Only**: Core functionality only

### 2. **ESP32-S3 Advantages**

- ✅ **More Memory**: ~300KB available heap
- ✅ **Dual Core**: Better multitasking
- ✅ **PSRAM Support**: Expandable memory
- ✅ **Full Feature Set**: All features supported

### 3. **CC1101 Dependencies**

- ❌ **No Built-in RF**: ESP32 lacks SubGHZ radio
- ❌ **SPI Required**: Must use SPI interface
- ⚠️ **Pin Constraints**: Requires 6+ GPIO pins
- ⚠️ **Power Consumption**: Additional 20-50mA

---

## Wiring Guide (External CC1101)

### 1. **Standard SPI Connection**

```
CC1101    ESP32 GPIO    Function
------    ----------    --------
VCC   →   3.3V         Power
GND   →   GND          Ground
SCK   →   GPIO18       SPI Clock (configurable)
MOSI  →   GPIO23       SPI Master Out
MISO  →   GPIO19       SPI Master In
CS    →   GPIO5        Chip Select (configurable)
GDO0  →   GPIO2        Data Out (interrupt)
GDO2  →   GPIO4        Data Out (optional)
```

### 2. **Antenna Connection**

```
CC1101 Antenna Requirements:
- 433MHz: ~17.3cm wire antenna
- 868MHz: ~8.2cm wire antenna
- 915MHz: ~7.8cm wire antenna

Optimal: Use helical or PCB antenna for better performance
```

---

## Configuration Recommendations

### 1. **T-Embed CC1101 (Optimal)**

```cpp
// Recommended configuration for best performance
#define SUBGHZ_MODE_FULL_FEATURED
#define SUBGHZ_ENABLE_ALL_PROTOCOLS
#define SUBGHZ_ENABLE_ALL_ATTACKS
#define SUBGHZ_MULTI_DEVICE_SUPPORT
#define SUBGHZ_HIGH_PERFORMANCE_MODE
```

### 2. **ESP32-S3 + External CC1101 (Standard)**

```cpp
// Balanced configuration for good performance
#define SUBGHZ_MODE_STANDARD
#define SUBGHZ_ENABLE_COMMON_PROTOCOLS
#define SUBGHZ_ENABLE_BASIC_ATTACKS
#define SUBGHZ_SINGLE_DEVICE_MODE
#define SUBGHZ_NORMAL_PERFORMANCE_MODE
```

### 3. **ESP32 + External CC1101 (Basic)**

```cpp
// Minimal configuration for legacy support
#define SUBGHZ_MODE_MINIMAL
#define SUBGHZ_ENABLE_STATIC_PROTOCOLS_ONLY
#define SUBGHZ_ENABLE_REPLAY_ONLY
#define SUBGHZ_SINGLE_OPERATION_MODE
#define SUBGHZ_LOW_PERFORMANCE_MODE
```

---

## Implementation Priority

### Phase 1: Core Support

1. ✅ **LilyGo T-Embed CC1101** - Primary target
2. ✅ **ESP32-S3 + External CC1101** - Secondary target

### Phase 2: Extended Support

3. ⚠️ **ESP32 + External CC1101** - Basic support
4. ⚠️ **Alternative RF modules** - Future consideration

### Phase 3: Optimization

5. 🔄 **Performance optimization** for all platforms
6. 🔄 **Memory optimization** for constrained devices
7. 🔄 **Power optimization** for battery operation

---

This hardware compatibility matrix ensures SubGHZ integration targets the most capable platforms first while maintaining backward compatibility where feasible.
