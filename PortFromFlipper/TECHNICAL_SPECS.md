# Technical Specifications - SubGHZ Integration

## Overview

This document provides detailed technical specifications for the SubGHZ module integration into Bruce firmware, including performance benchmarks, resource allocation, and implementation constraints.

---

## System Requirements

### 1. **Minimum Hardware Requirements**

| Component     | Minimum         | Recommended       | Optimal           |
| ------------- | --------------- | ----------------- | ----------------- |
| **MCU**       | ESP32 (240MHz)  | ESP32-S3 (240MHz) | ESP32-S3 (240MHz) |
| **RAM**       | 200KB available | 300KB available   | 400KB+ available  |
| **Flash**     | 2MB available   | 4MB available     | 8MB+ available    |
| **RF Module** | External CC1101 | External CC1101   | Built-in CC1101   |
| **GPIO Pins** | 6 pins minimum  | 8 pins preferred  | Dedicated SPI bus |

### 2. **Software Dependencies**

```cpp
// Required libraries and frameworks
#include <Arduino.h>              // Arduino framework
#include <freertos/FreeRTOS.h>    // Real-time OS
#include <driver/spi_master.h>    // SPI driver
#include <esp_timer.h>            // High-precision timing
#include <esp_log.h>              // Logging system

// Bruce framework dependencies
#include <MenuItemInterface.h>    // Menu system
#include <globals.h>              // Global configuration
#include <core/display.h>         // Display management
```

---

## Performance Specifications

### 1. **Signal Processing Performance**

| Operation             | ESP32 | ESP32-S3 | Units         | Notes                |
| --------------------- | ----- | -------- | ------------- | -------------------- |
| **Signal Capture**    | 100   | 500      | samples/sec   | Raw signal sampling  |
| **Protocol Decode**   | 10    | 50       | signals/sec   | Average decode speed |
| **Signal Encode**     | 20    | 100      | signals/sec   | Signal generation    |
| **File I/O**          | 5     | 15       | files/sec     | SD card operations   |
| **Memory Allocation** | 1ms   | 0.5ms    | per operation | Heap allocation time |

### 2. **Real-Time Constraints**

```cpp
// Timing specifications
#define SUBGHZ_MAX_DECODE_TIME_MS       100    // Maximum decode time
#define SUBGHZ_MAX_ENCODE_TIME_MS       50     // Maximum encode time
#define SUBGHZ_INTERRUPT_RESPONSE_US    10     // GDO0 interrupt response
#define SUBGHZ_SPI_TRANSACTION_US       5      // SPI transaction time
#define SUBGHZ_CONTEXT_SWITCH_US        20     // Task switching overhead

// Frequency specifications
#define SUBGHZ_MIN_FREQUENCY_HZ         300000000   // 300 MHz
#define SUBGHZ_MAX_FREQUENCY_HZ         928000000   // 928 MHz
#define SUBGHZ_FREQUENCY_STEP_HZ        396         // Frequency resolution
#define SUBGHZ_MAX_DEVIATION_HZ         380000      // Maximum deviation
```

### 3. **Memory Performance**

| Resource Type      | ESP32 Allocation | ESP32-S3 Allocation | Max Usage      |
| ------------------ | ---------------- | ------------------- | -------------- |
| **Heap Memory**    | 40KB             | 80KB                | Dynamic        |
| **Stack Memory**   | 8KB              | 12KB                | Per task       |
| **DMA Buffers**    | 2KB              | 4KB                 | SPI operations |
| **Signal Buffers** | 4KB              | 8KB                 | Configurable   |
| **Protocol Cache** | 5KB              | 15KB                | LRU cache      |

---

## Resource Allocation

### 1. **Memory Map**

```cpp
// Memory allocation strategy
typedef struct {
    // Core system memory
    void* furi_heap_pool;           // 15KB - Furi compatibility layer
    void* protocol_manager_pool;    // 10KB - Protocol management
    void* signal_buffer_pool;       // 8KB  - Signal buffering
    void* attack_engine_pool;       // 12KB - Attack implementations

    // Protocol-specific memory
    void* static_protocol_pool;     // 8KB  - Static protocols
    void* dynamic_protocol_pool;    // 15KB - Dynamic protocols
    void* crypto_work_pool;         // 5KB  - Cryptographic operations

    // I/O and communication
    void* file_io_buffer;          // 4KB  - File operations
    void* network_buffer;          // 3KB  - Multi-device coordination

    // Total allocation: ~80KB for ESP32-S3, ~40KB for ESP32
} subghz_memory_map_t;
```

### 2. **Task Allocation**

```cpp
// FreeRTOS task configuration
typedef struct {
    const char* name;
    uint32_t stack_size;
    UBaseType_t priority;
    BaseType_t core_id;
} task_config_t;

static const task_config_t subghz_tasks[] = {
    {"SubGHZ_Worker",    8192,  5, 1},  // Main worker task
    {"SubGHZ_RX",        4096,  6, 1},  // Reception handler
    {"SubGHZ_TX",        4096,  6, 1},  // Transmission handler
    {"SubGHZ_Decoder",   6144,  4, 0},  // Protocol decoder
    {"SubGHZ_Attack",    8192,  3, 0},  // Attack engine
    {"SubGHZ_FileIO",    3072,  2, 0},  // File operations
};

#define SUBGHZ_TASK_COUNT (sizeof(subghz_tasks) / sizeof(task_config_t))
```

### 3. **Interrupt Allocation**

```cpp
// GPIO interrupt configuration
typedef struct {
    gpio_num_t pin;
    gpio_int_type_t type;
    gpio_isr_t handler;
    int priority;
} interrupt_config_t;

static const interrupt_config_t subghz_interrupts[] = {
    {CC1101_GDO0_PIN, GPIO_INTR_POSEDGE, gdo0_isr_handler, 5},
    {CC1101_GDO2_PIN, GPIO_INTR_NEGEDGE, gdo2_isr_handler, 4},
};
```

---

## Protocol Specifications

### 1. **Supported Protocols Matrix**

| Protocol       | Type    | Frequency   | Modulation | Complexity | Memory Usage | Priority |
| -------------- | ------- | ----------- | ---------- | ---------- | ------------ | -------- |
| **Princeton**  | Static  | 433.92 MHz  | OOK/ASK    | Low        | 1.5KB        | High     |
| **CAME**       | Static  | 433.92 MHz  | OOK/ASK    | Low        | 1.8KB        | High     |
| **Nice**       | Static  | 433.92 MHz  | OOK/ASK    | Medium     | 2.1KB        | High     |
| **Linear**     | Static  | 318/390 MHz | OOK/ASK    | Low        | 1.6KB        | Medium   |
| **KeeLoq**     | Dynamic | 433.92 MHz  | OOK/ASK    | High       | 4.2KB        | High     |
| **SecPlus v1** | Dynamic | 315/390 MHz | OOK/ASK    | Medium     | 2.8KB        | Medium   |
| **SecPlus v2** | Dynamic | 315/390 MHz | OOK/ASK    | High       | 3.5KB        | Medium   |
| **KIA**        | Dynamic | 433.92 MHz  | OOK/ASK    | High       | 3.8KB        | Low      |
| **Star Line**  | Dynamic | 433.92 MHz  | OOK/ASK    | Medium     | 2.9KB        | Low      |
| **Megacode**   | Dynamic | 318 MHz     | OOK/ASK    | High       | 4.1KB        | Low      |

### 2. **Protocol Loading Strategy**

```cpp
// Dynamic protocol loading based on priority and memory
typedef enum {
    PROTOCOL_PRIORITY_CRITICAL = 0,  // Always loaded (Princeton, CAME)
    PROTOCOL_PRIORITY_HIGH = 1,      // Loaded when possible (KeeLoq, Nice)
    PROTOCOL_PRIORITY_MEDIUM = 2,    // Loaded on demand (SecPlus)
    PROTOCOL_PRIORITY_LOW = 3,       // Loaded only when requested (KIA)
} protocol_priority_t;

// Protocol loading limits by device
#ifdef ESP32_S3
#define MAX_CONCURRENT_PROTOCOLS 15
#define MAX_CRITICAL_PROTOCOLS   8
#define MAX_HIGH_PROTOCOLS       4
#define MAX_MEDIUM_PROTOCOLS     2
#define MAX_LOW_PROTOCOLS        1
#else // ESP32
#define MAX_CONCURRENT_PROTOCOLS 8
#define MAX_CRITICAL_PROTOCOLS   5
#define MAX_HIGH_PROTOCOLS       2
#define MAX_MEDIUM_PROTOCOLS     1
#define MAX_LOW_PROTOCOLS        0
#endif
```

### 3. **Protocol Performance Characteristics**

```cpp
// Decode performance specifications
typedef struct {
    const char* name;
    uint32_t min_bits;
    uint32_t max_bits;
    uint32_t avg_decode_time_us;
    uint32_t max_decode_time_us;
    float accuracy_percentage;
} protocol_performance_t;

static const protocol_performance_t protocol_specs[] = {
    {"Princeton", 24, 24, 150, 500, 99.5},
    {"CAME",      12, 24, 120, 400, 99.2},
    {"KeeLoq",    64, 69, 800, 2000, 98.8},
    {"SecPlus_v2", 62, 62, 600, 1500, 98.5},
    // ... more protocols
};
```

---

## Attack Implementation Specifications

### 1. **RollJam Attack Specifications**

```cpp
// RollJam attack parameters
typedef struct {
    uint32_t jam_frequency_hz;           // Jamming frequency
    uint32_t jam_duration_ms;            // Jamming duration per signal
    uint32_t capture_window_ms;          // Signal capture window
    uint32_t max_captures;               // Maximum captured signals
    uint32_t replay_delay_ms;            // Delay before replay
    float jam_power_dbm;                 // Jamming power level
} rolljam_config_t;

static const rolljam_config_t rolljam_defaults = {
    .jam_frequency_hz = 433920000,       // 433.92 MHz
    .jam_duration_ms = 2000,             // 2 second jam
    .capture_window_ms = 10000,          // 10 second capture
    .max_captures = 10,                  // Up to 10 signals
    .replay_delay_ms = 5000,             // 5 second delay
    .jam_power_dbm = 10.0,               // 10 dBm jamming power
};
```

### 2. **RollBack Attack Specifications**

```cpp
// RollBack attack parameters
typedef struct {
    uint32_t counter_window_size;        // Counter tolerance window
    uint32_t max_prediction_attempts;    // Maximum prediction tries
    uint32_t seed_bruteforce_depth;      // Seed search depth
    uint32_t key_analysis_timeout_ms;    // Key analysis timeout
    bool enable_statistical_analysis;    // Statistical crypto analysis
} rollback_config_t;

static const rollback_config_t rollback_defaults = {
    .counter_window_size = 65536,        // 16-bit counter window
    .max_prediction_attempts = 1000,     // 1000 predictions max
    .seed_bruteforce_depth = 10000,      // 10K seed attempts
    .key_analysis_timeout_ms = 30000,    // 30 second timeout
    .enable_statistical_analysis = true, // Enable advanced analysis
};
```

### 3. **Multi-Device Coordination**

```cpp
// Multi-device attack coordination
typedef struct {
    uint8_t device_id;                   // Unique device identifier
    uint8_t role;                        // Device role (jammer/capturer/replayer)
    uint32_t sync_frequency_hz;          // Synchronization frequency
    uint32_t coordination_timeout_ms;    // Coordination timeout
    bool enable_mesh_networking;         // Enable mesh coordination
} coordination_config_t;

typedef enum {
    DEVICE_ROLE_MASTER = 0,              // Master coordinator
    DEVICE_ROLE_JAMMER = 1,              // Jamming device
    DEVICE_ROLE_CAPTURER = 2,            // Signal capture device
    DEVICE_ROLE_REPLAYER = 3,            // Signal replay device
    DEVICE_ROLE_ANALYZER = 4,            // Signal analysis device
} device_role_t;
```

---

## Communication Protocols

### 1. **Multi-Device Communication**

```cpp
// Inter-device communication protocol
typedef struct {
    uint8_t magic[4];                    // Protocol magic bytes
    uint8_t version;                     // Protocol version
    uint8_t command;                     // Command type
    uint8_t device_id;                   // Source device ID
    uint8_t target_id;                   // Target device ID
    uint16_t payload_length;             // Payload length
    uint32_t timestamp;                  // Synchronization timestamp
    uint8_t checksum;                    // Message checksum
    uint8_t payload[];                   // Variable payload
} inter_device_message_t;

#define INTERDEV_MAGIC {0x42, 0x52, 0x55, 0x43} // "BRUC"
#define INTERDEV_VERSION 1

// Command types
#define CMD_SYNC_REQUEST     0x01        // Synchronization request
#define CMD_SYNC_RESPONSE    0x02        // Synchronization response
#define CMD_ATTACK_START     0x10        // Start attack command
#define CMD_ATTACK_STOP      0x11        // Stop attack command
#define CMD_SIGNAL_CAPTURED  0x20        // Signal captured notification
#define CMD_ATTACK_RESULT    0x30        // Attack result report
```

### 2. **WiFi Coordination Protocol**

```cpp
// WiFi-based device coordination
typedef struct {
    char ssid[32];                       // Coordination network SSID
    char password[64];                   // Network password
    uint16_t port;                       // Communication port
    uint32_t discovery_timeout_ms;       // Device discovery timeout
    uint8_t max_devices;                 // Maximum coordinated devices
} wifi_coordination_t;

static const wifi_coordination_t wifi_defaults = {
    .ssid = "Bruce-SubGHZ-Coord",
    .password = "RollJamAttack2025",
    .port = 8433,
    .discovery_timeout_ms = 30000,
    .max_devices = 8,
};
```

---

## File Format Specifications

### 1. **SubGHZ File Format (.sub)**

```cpp
// SubGHZ file format structure
typedef struct {
    char header[16];                     // File header "FlipperSubGhz"
    uint8_t version;                     // File format version
    uint32_t frequency;                  // Signal frequency in Hz
    uint8_t modulation;                  // Modulation type
    uint32_t sample_rate;                // Sample rate in Hz
    uint32_t data_length;                // Signal data length
    char protocol[32];                   // Protocol name
    uint32_t timestamp;                  // Capture timestamp
    uint8_t metadata[];                  // Protocol-specific metadata
    uint8_t signal_data[];               // Raw signal data
} subghz_file_t;

// Supported modulations
#define MOD_OOK_ASK      0x01            // On-Off Keying / ASK
#define MOD_2FSK         0x02            // 2-FSK modulation
#define MOD_4FSK         0x03            // 4-FSK modulation
#define MOD_GFSK         0x04            // Gaussian FSK
#define MOD_MSK          0x05            // Minimum Shift Keying
```

### 2. **Attack Log Format (.log)**

```cpp
// Attack log file format
typedef struct {
    uint32_t timestamp;                  // Attack timestamp
    uint8_t attack_type;                 // Type of attack performed
    uint8_t target_protocol;             // Target protocol
    uint32_t frequency;                  // Attack frequency
    uint16_t duration_ms;                // Attack duration
    uint8_t success;                     // Attack success flag
    uint16_t captured_signals;           // Number of captured signals
    uint32_t data_offset;                // Offset to attack data
    uint32_t data_length;                // Length of attack data
} attack_log_entry_t;

#define ATTACK_TYPE_ROLLJAM    0x01
#define ATTACK_TYPE_ROLLBACK   0x02
#define ATTACK_TYPE_REPLAY     0x03
#define ATTACK_TYPE_BRUTEFORCE 0x04
#define ATTACK_TYPE_JAMMING    0x05
```

---

## Quality Assurance Specifications

### 1. **Testing Requirements**

```cpp
// Test coverage requirements
#define MIN_CODE_COVERAGE_PERCENT     85   // Minimum code coverage
#define MIN_PROTOCOL_TEST_SIGNALS     100  // Test signals per protocol
#define MIN_ATTACK_SUCCESS_RATE       80   // Minimum attack success rate
#define MAX_MEMORY_LEAK_BYTES         1024 // Maximum memory leak tolerance
#define MAX_CRASH_RECOVERY_TIME_MS    5000 // Maximum crash recovery time

// Performance benchmarks
typedef struct {
    const char* test_name;
    uint32_t max_execution_time_ms;
    uint32_t max_memory_usage_kb;
    float min_success_rate;
} performance_benchmark_t;

static const performance_benchmark_t benchmarks[] = {
    {"Princeton_Decode_Speed", 5, 2, 99.0},
    {"KeeLoq_Decode_Speed", 20, 5, 95.0},
    {"RollJam_Attack_Speed", 10000, 15, 85.0},
    {"File_Save_Speed", 1000, 8, 99.5},
    {"Memory_Stress_Test", 60000, 80, 100.0},
};
```

### 2. **Error Handling Specifications**

```cpp
// Error codes and handling
typedef enum {
    SUBGHZ_OK = 0,
    SUBGHZ_ERROR_MEMORY = -1,            // Memory allocation failure
    SUBGHZ_ERROR_HARDWARE = -2,          // Hardware communication error
    SUBGHZ_ERROR_PROTOCOL = -3,          // Protocol decode error
    SUBGHZ_ERROR_FILE_IO = -4,           // File operation error
    SUBGHZ_ERROR_TIMEOUT = -5,           // Operation timeout
    SUBGHZ_ERROR_INVALID_PARAM = -6,     // Invalid parameter
    SUBGHZ_ERROR_NOT_SUPPORTED = -7,     // Feature not supported
    SUBGHZ_ERROR_BUSY = -8,              // Resource busy
} subghz_error_t;

// Error recovery mechanisms
typedef struct {
    subghz_error_t error_code;
    void (*recovery_handler)(void);
    uint32_t max_recovery_attempts;
    uint32_t recovery_delay_ms;
} error_recovery_t;
```

---

## Security Specifications

### 1. **Responsible Use Framework**

```cpp
// Security and ethical use constraints
#define ATTACK_SESSION_MAX_DURATION_MS   300000  // 5 minute max session
#define ATTACK_COOLDOWN_PERIOD_MS        600000  // 10 minute cooldown
#define MAX_DAILY_ATTACK_SESSIONS        10      // Max 10 sessions per day
#define REQUIRE_USER_CONFIRMATION        true    // Require attack confirmation

// Logging requirements for security research
typedef struct {
    bool log_all_attacks;                // Log all attack attempts
    bool log_signal_metadata_only;       // Don't log actual signal data
    bool require_research_justification; // Require justification
    bool enable_audit_trail;             // Enable audit logging
} security_config_t;
```

### 2. **Legal Compliance**

```cpp
// Legal compliance features
#define ENABLE_FREQUENCY_RESTRICTIONS    true    // Respect regional limits
#define ENABLE_POWER_RESTRICTIONS        true    // Respect power limits
#define ENABLE_JAMMING_RESTRICTIONS      true    // Restrict jamming features
#define REQUIRE_EDUCATIONAL_WARNING      true    // Show educational warnings

// Regional frequency restrictions
typedef struct {
    const char* region_code;
    uint32_t min_frequency_hz;
    uint32_t max_frequency_hz;
    float max_power_dbm;
    bool jamming_allowed;
} regional_restrictions_t;
```

---

This technical specification provides the detailed foundation needed for implementing the SubGHZ integration with precise performance targets, resource constraints, and quality requirements.
