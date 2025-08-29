#ifndef __SUBGHZ_TEST_H__
#define __SUBGHZ_TEST_H__

#include <stdbool.h>

/**
 * @brief SubGHz system testing utilities
 *
 * Comprehensive test suite for SubGHz module components
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Test memory management system
 * @return true if all memory tests pass
 */
bool subghz_test_memory_system(void);

/**
 * @brief Test core SubGHz system
 * @return true if core system tests pass
 */
bool subghz_test_core_system(void);

/**
 * @brief Test protocol detection system
 * @return true if protocol tests pass
 */
bool subghz_test_protocol_system(void);

/**
 * @brief Test transmitter and receiver
 * @return true if TX/RX tests pass
 */
bool subghz_test_transmitter_receiver(void);

/**
 * @brief Test UI system
 * @return true if UI tests pass
 */
bool subghz_test_ui_system(void);

/**
 * @brief Run comprehensive system test
 * @return true if all tests pass
 */
bool subghz_run_comprehensive_test(void);

/**
 * @brief Start test task (async)
 */
void subghz_start_test_task(void);

#ifdef __cplusplus
}
#endif

#endif // __SUBGHZ_TEST_H__
