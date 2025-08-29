#ifndef __SUBGHZ_MEMORY_H__
#define __SUBGHZ_MEMORY_H__

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SubGHz Memory Management
 *
 * Adapted from Furi memory management for SubGHz operations
 * Provides safe memory allocation with tracking and bounds checking
 */

// Memory pool types for SubGHz operations
typedef enum {
    SUBGHZ_POOL_PROTOCOL, // Protocol instances
    SUBGHZ_POOL_SIGNAL,   // Signal buffers
    SUBGHZ_POOL_STRING,   // String operations
    SUBGHZ_POOL_TEMP,     // Temporary allocations
    SUBGHZ_POOL_COUNT
} SubGhzMemoryPool;

// Memory allocation flags
#define SUBGHZ_MEM_FLAG_ZERO (1 << 0)     // Zero initialize
#define SUBGHZ_MEM_FLAG_TRACE (1 << 1)    // Enable tracing
#define SUBGHZ_MEM_FLAG_CRITICAL (1 << 2) // Critical allocation

// Memory statistics
typedef struct {
    size_t total_allocated;
    size_t total_freed;
    size_t current_usage;
    size_t peak_usage;
    uint32_t allocation_count;
    uint32_t free_count;
    uint32_t allocation_failures;
} SubGhzMemoryStats;

/**
 * @brief Initialize SubGHz memory management
 * @return true if successful, false otherwise
 */
bool subghz_memory_init(void);

/**
 * @brief Deinitialize SubGHz memory management
 */
void subghz_memory_deinit(void);

/**
 * @brief Allocate memory from SubGHz pool
 * @param pool Memory pool type
 * @param size Size to allocate
 * @param flags Allocation flags
 * @return Pointer to allocated memory or NULL on failure
 */
void *subghz_malloc(SubGhzMemoryPool pool, size_t size, uint32_t flags);

/**
 * @brief Reallocate memory in SubGHz pool
 * @param pool Memory pool type
 * @param ptr Previous pointer
 * @param size New size
 * @param flags Allocation flags
 * @return Pointer to reallocated memory or NULL on failure
 */
void *subghz_realloc(SubGhzMemoryPool pool, void *ptr, size_t size, uint32_t flags);

/**
 * @brief Free memory from SubGHz pool
 * @param pool Memory pool type
 * @param ptr Pointer to free
 */
void subghz_free(SubGhzMemoryPool pool, void *ptr);

/**
 * @brief Allocate and zero-initialize memory
 * @param pool Memory pool type
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to allocated memory or NULL on failure
 */
void *subghz_calloc(SubGhzMemoryPool pool, size_t count, size_t size);

/**
 * @brief Get memory statistics for pool
 * @param pool Memory pool type
 * @param stats Output statistics structure
 * @return true if successful
 */
bool subghz_memory_get_stats(SubGhzMemoryPool pool, SubGhzMemoryStats *stats);

/**
 * @brief Get total memory statistics
 * @param stats Output statistics structure
 * @return true if successful
 */
bool subghz_memory_get_total_stats(SubGhzMemoryStats *stats);

/**
 * @brief Check memory integrity
 * @param pool Memory pool type (or SUBGHZ_POOL_COUNT for all)
 * @return true if memory is intact
 */
bool subghz_memory_check_integrity(SubGhzMemoryPool pool);

/**
 * @brief Force garbage collection
 * @param pool Memory pool type (or SUBGHZ_POOL_COUNT for all)
 */
void subghz_memory_gc(SubGhzMemoryPool pool);

/**
 * @brief Get available memory for pool
 * @param pool Memory pool type
 * @return Available bytes
 */
size_t subghz_memory_get_available(SubGhzMemoryPool pool);

/**
 * @brief Set memory pressure callback
 * @param callback Function to call on memory pressure
 */
void subghz_memory_set_pressure_callback(void (*callback)(SubGhzMemoryPool pool));

/**
 * @brief Print memory statistics to serial/log
 */
void subghz_memory_print_stats(void);

// Convenience macros
#define SUBGHZ_MALLOC(pool, size) subghz_malloc(pool, size, 0)
#define SUBGHZ_MALLOC_ZERO(pool, size) subghz_malloc(pool, size, SUBGHZ_MEM_FLAG_ZERO)
#define SUBGHZ_CALLOC(pool, count, size) subghz_calloc(pool, count, size)
#define SUBGHZ_FREE(pool, ptr) subghz_free(pool, ptr)

#ifdef __cplusplus
}
#endif

#endif // __SUBGHZ_MEMORY_H__
