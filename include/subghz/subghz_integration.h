#ifndef __SUBGHZ_INTEGRATION_H__
#define __SUBGHZ_INTEGRATION_H__

/**
 * @brief SubGHz Module Integration for Bruce Framework
 *
 * This header provides easy integration of the complete SubGHz module
 * into the Bruce framework. Include this file to get access to all
 * SubGHz functionality.
 */

// Core SubGHz components
#include "subghz/subghz_config.h"
#include "subghz/subghz_core.h"
#include "subghz/subghz_device.h"
#include "subghz/subghz_memory.h"
#include "subghz/subghz_types.h"

// Protocol system
#include "subghz/protocols/subghz_protocol_princeton.h"
#include "subghz/subghz_protocol_registry.h"

// Signal processing
#include "subghz/subghz_receiver.h"
#include "subghz/subghz_transmitter.h"

// User interface
#include "subghz/subghz_ui.h"

// Testing utilities
#include "subghz/subghz_test.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the complete SubGHz system
 *
 * This function initializes all SubGHz components and should be called
 * during Bruce framework startup.
 *
 * @return true if initialization successful
 */
bool subghz_system_init(void);

/**
 * @brief Deinitialize the SubGHz system
 *
 * Cleanup all SubGHz resources. Called during shutdown.
 */
void subghz_system_deinit(void);

/**
 * @brief Get SubGHz system status
 *
 * @return true if system is initialized and ready
 */
bool subghz_system_is_ready(void);

/**
 * @brief Quick start function for testing SubGHz
 *
 * Convenient function for quick testing and demonstrations
 */
void subghz_quick_start_demo(void);

/**
 * @brief Register SubGHz menu with Bruce main menu
 *
 * Called automatically during system initialization
 */
void subghz_register_menu(void);

#ifdef __cplusplus
}
#endif

// Quick access macros for common operations
#define SUBGHZ_INIT() subghz_system_init()
#define SUBGHZ_READY() subghz_system_is_ready()
#define SUBGHZ_DEMO() subghz_quick_start_demo()

#endif // __SUBGHZ_INTEGRATION_H__
