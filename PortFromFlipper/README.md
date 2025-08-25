# SubGHZ Port from Flipper Unleashed Firmware

## Overview

This directory contains the ported SubGHZ functionality from the [Unleashed Firmware](https://github.com/Eng1n33r/unleashed-firmware/) repository, specifically designed for integration into the Bruce framework. The goal is to migrate and adapt the advanced SubGHZ capabilities from Flipper Zero's Unleashed firmware to enhance Bruce's research and RF analysis capabilities.

## Project Objectives

The primary objectives of this port are:

- **Enhanced RF Capabilities**: Bring advanced SubGHZ protocol support to Bruce
- **Protocol Expansion**: Support for a wider range of SubGHZ protocols and devices
- **Research Enhancement**: Elevate Bruce to a new level of RF research and analysis capabilities
- **Furi-like Framework**: Implement a similar event-driven framework structure for better code organization
- **Driver Integration**: Port CC1101 and other RF drivers for improved hardware support

## Directory Structure

```
PortFromFlipper/
├── applications/           # Application layer components
│   ├── drivers/           # Hardware driver applications
│   │   ├── subghz/       # SubGHZ driver application
│   │   │   └── cc1101_ext/   # External CC1101 driver extensions
│   └── main/             # Main application modules
│       ├── subghz/       # Core SubGHZ application
│       │   ├── helpers/  # Helper functions and utilities
│       │   ├── resources/# SubGHZ resources and configurations
│       │   ├── scenes/   # UI scenes and state management
│       │   └── views/    # UI view components
│       └── subghz_remote/# SubGHZ remote control functionality
├── furi/                 # Furi framework core
│   └── core/            # Core Furi components (event loops, threads, etc.)
└── lib/                 # Library components
    ├── drivers/         # Hardware driver libraries
    └── subghz/         # SubGHZ protocol libraries
        ├── blocks/     # Protocol building blocks
        ├── devices/    # Device-specific implementations
        └── protocols/ # Protocol implementations
```

## Key Components

### 1. Furi Framework Core (`furi/`)

The Furi framework provides the foundational architecture used in Flipper firmware:

- **Event System**: Advanced event loop management for asynchronous operations
- **Threading**: Thread management and synchronization primitives
- **Memory Management**: Heap management and memory utilities
- **Logging**: Comprehensive logging system
- **String Handling**: Enhanced string manipulation utilities
- **Inter-Process Communication**: Message queues, mutexes, semaphores

**Key Files:**

- `event_loop.c/h`: Main event loop implementation
- `thread.c/h`: Thread management
- `memmgr.c/h`: Memory management
- `log.c/h`: Logging system
- `string.c/h`: String utilities

### 2. SubGHZ Library (`lib/subghz/`)

Comprehensive SubGHZ protocol support including:

- **Protocol Registry**: Central registry for all supported protocols
- **Signal Processing**: Advanced signal encoding/decoding
- **File Operations**: SubGHZ file format support
- **Environment Management**: RF environment configuration
- **Worker Threads**: Background signal processing

**Protocol Support:**

- Rolling code protocols (KeeLoq, etc.)
- Static code protocols
- Weather station protocols
- Car key protocols
- Garage door protocols
- And many more...

### 3. Hardware Drivers (`lib/drivers/`)

Low-level hardware abstraction for:

- **CC1101**: Advanced CC1101 transceiver driver
- **ST25R3916**: NFC/RFID driver
- **BQ25896/BQ27220**: Power management
- **LP5562**: LED controller
- **SK6805**: RGB LED strip controller

### 4. Applications (`applications/`)

High-level application components:

- **SubGHZ CLI**: Command-line interface for SubGHZ operations
- **SubGHZ GUI**: Graphical user interface components
- **Scene Management**: State machine for UI navigation
- **History Management**: Signal capture and replay history

## Integration Strategy

### Phase 1: Core Framework

1. **Furi Core Integration**: Adapt Furi's event system to work with Bruce's existing architecture
2. **Memory Management**: Integrate Furi's memory management with Bruce's system
3. **Threading**: Implement Furi's threading model alongside Bruce's current approach

### Phase 2: Driver Layer

1. **CC1101 Driver**: Port and integrate the advanced CC1101 driver
2. **Hardware Abstraction**: Create Bruce-compatible hardware abstraction layer
3. **Pin Configuration**: Adapt driver pin configurations for supported Bruce devices

### Phase 3: Protocol Implementation

1. **Protocol Registry**: Implement the protocol registry system
2. **Signal Processing**: Port signal encoding/decoding capabilities
3. **File Format Support**: Add SubGHZ file format support to Bruce

### Phase 4: Application Layer

1. **UI Integration**: Integrate SubGHZ UI components with Bruce's menu system
2. **CLI Commands**: Add SubGHZ CLI commands to Bruce's command interface
3. **Scene Management**: Implement scene-based navigation for SubGHZ features

## Technical Challenges

### 1. Architecture Differences

- **Event System**: Adapting Flipper's event-driven architecture to Bruce's structure
- **Resource Management**: Integrating Furi's resource management with Bruce's system
- **Hardware Abstraction**: Bridging different hardware abstraction approaches

### 2. Memory Constraints

- **Heap Management**: Optimizing memory usage for embedded environments
- **Buffer Management**: Efficient signal buffer management
- **Protocol Storage**: Managing multiple protocol implementations in limited memory

### 3. Real-time Requirements

- **Signal Timing**: Maintaining precise timing for RF signal generation
- **Interrupt Handling**: Proper interrupt management for real-time operations
- **Thread Synchronization**: Ensuring thread-safe operations

## Expected Benefits

### Enhanced Capabilities

- **Advanced Protocol Support**: Access to 50+ SubGHZ protocols
- **Improved Signal Quality**: Better signal generation and reception
- **Professional Features**: Advanced analysis and debugging tools

### Research Applications

- **Protocol Analysis**: Deep analysis of unknown protocols
- **Signal Intelligence**: Advanced signal intelligence capabilities
- **Security Research**: Enhanced security research tools

### Code Quality

- **Modular Architecture**: Clean, modular code structure
- **Event-Driven Design**: Responsive, event-driven architecture
- **Comprehensive Testing**: Well-tested protocol implementations

## Implementation Notes

### Compilation Considerations

- Some Furi components may need adaptation for different compiler environments
- Hardware-specific code will require platform abstraction
- Memory management may need optimization for different target devices

### Dependencies

- Integration with Bruce's existing TFT display system
- Compatibility with Bruce's menu and navigation system
- Coordination with Bruce's existing RF modules

### Testing Strategy

- Protocol validation using known test vectors
- Hardware compatibility testing across Bruce-supported devices
- Performance benchmarking against original Flipper implementation

## Contributing

When contributing to this port:

1. Maintain compatibility with Bruce's existing architecture
2. Follow Bruce's coding standards and conventions
3. Ensure proper error handling and resource cleanup
4. Document any deviations from the original Flipper implementation
5. Test thoroughly on multiple Bruce-compatible devices

## Future Enhancements

### Advanced Features

- **Custom Protocol Support**: Framework for adding custom protocols
- **Signal Analysis**: Advanced signal analysis and visualization
- **Machine Learning**: ML-based protocol detection and analysis

### Integration Opportunities

- **NFC Integration**: Combine SubGHZ with NFC capabilities
- **WiFi Coordination**: Coordinate SubGHZ with WiFi attacks
- **Bluetooth Integration**: Multi-protocol attack scenarios

## License

This port maintains compatibility with the original Unleashed Firmware licensing terms while adapting to Bruce's project structure and requirements.

---

**Note**: This is an ongoing project aimed at significantly enhancing Bruce's RF research capabilities. The implementation will be done in phases to ensure stability and compatibility with the existing Bruce ecosystem.
