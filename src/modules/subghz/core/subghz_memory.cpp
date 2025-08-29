#include "subghz/subghz_memory.h"
#include "core/display.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "SubGhzMemory";

// Memory pool configuration
#define SUBGHZ_PROTOCOL_POOL_SIZE (32 * 1024) // 32KB for protocol instances
#define SUBGHZ_SIGNAL_POOL_SIZE (64 * 1024)   // 64KB for signal buffers
#define SUBGHZ_STRING_POOL_SIZE (16 * 1024)   // 16KB for string operations
#define SUBGHZ_TEMP_POOL_SIZE (8 * 1024)      // 8KB for temporary allocations

// Memory pool structure
typedef struct {
    void *base_ptr;    // Base memory pointer
    size_t total_size; // Total pool size
    size_t used_size;  // Currently used size
    size_t peak_usage; // Peak usage
    uint32_t allocation_count;
    uint32_t free_count;
    uint32_t failures;
    bool initialized;
    portMUX_TYPE mutex; // Thread safety
} SubGhzMemoryPoolInfo;

// Memory allocation header for tracking
typedef struct __attribute__((packed)) {
    uint32_t magic;        // Magic number for integrity check
    SubGhzMemoryPool pool; // Which pool this belongs to
    size_t size;           // Allocated size
    uint32_t flags;        // Allocation flags
    uint64_t timestamp;    // Allocation timestamp
} SubGhzMemoryHeader;

#define SUBGHZ_MEMORY_MAGIC 0x53474856 // "SGHV"
#define SUBGHZ_MEMORY_HEADER_SIZE sizeof(SubGhzMemoryHeader)

// Global memory pools
static SubGhzMemoryPoolInfo memory_pools[SUBGHZ_POOL_COUNT] = {0};
static bool memory_system_initialized = false;
static void (*pressure_callback)(SubGhzMemoryPool) = NULL;

// Pool size configuration
static const size_t pool_sizes[SUBGHZ_POOL_COUNT] = {
    SUBGHZ_PROTOCOL_POOL_SIZE, SUBGHZ_SIGNAL_POOL_SIZE, SUBGHZ_STRING_POOL_SIZE, SUBGHZ_TEMP_POOL_SIZE
};

static const char *pool_names[SUBGHZ_POOL_COUNT] = {"Protocol", "Signal", "String", "Temp"};

bool subghz_memory_init(void) {
    if (memory_system_initialized) {
        ESP_LOGW(TAG, "Memory system already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing SubGHz memory management");

    // Initialize each memory pool
    for (int i = 0; i < SUBGHZ_POOL_COUNT; i++) {
        SubGhzMemoryPoolInfo *pool = &memory_pools[i];

        // Allocate pool memory
        pool->base_ptr = heap_caps_malloc(pool_sizes[i], MALLOC_CAP_8BIT);
        if (!pool->base_ptr) {
            ESP_LOGE(TAG, "Failed to allocate %s pool (%zu bytes)", pool_names[i], pool_sizes[i]);
            // Cleanup previously allocated pools
            for (int j = 0; j < i; j++) {
                if (memory_pools[j].base_ptr) {
                    heap_caps_free(memory_pools[j].base_ptr);
                    memory_pools[j].base_ptr = NULL;
                }
            }
            return false;
        }

        pool->total_size = pool_sizes[i];
        pool->used_size = 0;
        pool->peak_usage = 0;
        pool->allocation_count = 0;
        pool->free_count = 0;
        pool->failures = 0;
        pool->initialized = true;
        portMUX_INITIALIZE(&pool->mutex);

        ESP_LOGI(TAG, "Initialized %s pool: %zu bytes", pool_names[i], pool_sizes[i]);
    }

    memory_system_initialized = true;
    ESP_LOGI(TAG, "SubGHz memory management initialized successfully");
    return true;
}

void subghz_memory_deinit(void) {
    if (!memory_system_initialized) { return; }

    ESP_LOGI(TAG, "Deinitializing SubGHz memory management");

    for (int i = 0; i < SUBGHZ_POOL_COUNT; i++) {
        SubGhzMemoryPoolInfo *pool = &memory_pools[i];
        if (pool->base_ptr) {
            if (pool->used_size > 0) {
                ESP_LOGW(TAG, "%s pool has %zu bytes still allocated", pool_names[i], pool->used_size);
            }
            heap_caps_free(pool->base_ptr);
            pool->base_ptr = NULL;
            pool->initialized = false;
        }
    }

    memory_system_initialized = false;
    ESP_LOGI(TAG, "SubGHz memory management deinitialized");
}

static bool validate_pool(SubGhzMemoryPool pool) {
    if (pool >= SUBGHZ_POOL_COUNT) {
        ESP_LOGE(TAG, "Invalid memory pool: %d", pool);
        return false;
    }

    if (!memory_system_initialized) {
        ESP_LOGE(TAG, "Memory system not initialized");
        return false;
    }

    if (!memory_pools[pool].initialized) {
        ESP_LOGE(TAG, "Pool %s not initialized", pool_names[pool]);
        return false;
    }

    return true;
}

void *subghz_malloc(SubGhzMemoryPool pool, size_t size, uint32_t flags) {
    if (!validate_pool(pool) || size == 0) { return NULL; }

    SubGhzMemoryPoolInfo *pool_info = &memory_pools[pool];
    size_t total_size = size + SUBGHZ_MEMORY_HEADER_SIZE;

    // Check if allocation would exceed pool size
    if (total_size > pool_info->total_size) {
        ESP_LOGE(
            TAG,
            "Allocation too large for %s pool: %zu > %zu",
            pool_names[pool],
            total_size,
            pool_info->total_size
        );
        return NULL;
    }

    portENTER_CRITICAL(&pool_info->mutex);

    // Check available space
    if (pool_info->used_size + total_size > pool_info->total_size) {
        pool_info->failures++;
        portEXIT_CRITICAL(&pool_info->mutex);

        ESP_LOGW(
            TAG,
            "%s pool out of memory: need %zu, have %zu",
            pool_names[pool],
            total_size,
            pool_info->total_size - pool_info->used_size
        );

        // Call pressure callback if set
        if (pressure_callback) { pressure_callback(pool); }
        return NULL;
    }

    // Simple allocation from pool (linear allocator for now)
    void *ptr = (uint8_t *)pool_info->base_ptr + pool_info->used_size;
    SubGhzMemoryHeader *header = (SubGhzMemoryHeader *)ptr;

    // Fill header
    header->magic = SUBGHZ_MEMORY_MAGIC;
    header->pool = pool;
    header->size = size;
    header->flags = flags;
    header->timestamp = esp_timer_get_time();

    void *user_ptr = (uint8_t *)ptr + SUBGHZ_MEMORY_HEADER_SIZE;

    // Zero initialize if requested
    if (flags & SUBGHZ_MEM_FLAG_ZERO) { memset(user_ptr, 0, size); }

    // Update pool statistics
    pool_info->used_size += total_size;
    pool_info->allocation_count++;

    if (pool_info->used_size > pool_info->peak_usage) { pool_info->peak_usage = pool_info->used_size; }

    portEXIT_CRITICAL(&pool_info->mutex);

    if (flags & SUBGHZ_MEM_FLAG_TRACE) {
        ESP_LOGI(TAG, "Allocated %zu bytes from %s pool at %p", size, pool_names[pool], user_ptr);
    }

    return user_ptr;
}

void subghz_free(SubGhzMemoryPool pool, void *ptr) {
    if (!ptr || !validate_pool(pool)) { return; }

    // Get header
    SubGhzMemoryHeader *header = (SubGhzMemoryHeader *)((uint8_t *)ptr - SUBGHZ_MEMORY_HEADER_SIZE);

    // Validate header
    if (header->magic != SUBGHZ_MEMORY_MAGIC) {
        ESP_LOGE(TAG, "Invalid memory header magic at %p", ptr);
        return;
    }

    if (header->pool != pool) {
        ESP_LOGE(TAG, "Memory belongs to wrong pool: expected %d, got %d", pool, header->pool);
        return;
    }

    SubGhzMemoryPoolInfo *pool_info = &memory_pools[pool];
    portENTER_CRITICAL(&pool_info->mutex);

    // For now, just mark as freed (simple linear allocator)
    // In a more sophisticated implementation, we'd maintain a free list
    pool_info->free_count++;

    // Clear the memory for security
    memset(ptr, 0, header->size);
    header->magic = 0; // Invalidate header

    portEXIT_CRITICAL(&pool_info->mutex);

    if (header->flags & SUBGHZ_MEM_FLAG_TRACE) {
        ESP_LOGI(TAG, "Freed %zu bytes from %s pool at %p", header->size, pool_names[pool], ptr);
    }
}

void *subghz_calloc(SubGhzMemoryPool pool, size_t count, size_t size) {
    if (count == 0 || size == 0) { return NULL; }

    // Check for overflow
    if (count > SIZE_MAX / size) {
        ESP_LOGE(TAG, "Calloc overflow: count=%zu, size=%zu", count, size);
        return NULL;
    }

    size_t total_size = count * size;
    return subghz_malloc(pool, total_size, SUBGHZ_MEM_FLAG_ZERO);
}

bool subghz_memory_get_stats(SubGhzMemoryPool pool, SubGhzMemoryStats *stats) {
    if (!validate_pool(pool) || !stats) { return false; }

    SubGhzMemoryPoolInfo *pool_info = &memory_pools[pool];
    portENTER_CRITICAL(&pool_info->mutex);

    stats->total_allocated = pool_info->allocation_count;
    stats->total_freed = pool_info->free_count;
    stats->current_usage = pool_info->used_size;
    stats->peak_usage = pool_info->peak_usage;
    stats->allocation_count = pool_info->allocation_count;
    stats->free_count = pool_info->free_count;
    stats->allocation_failures = pool_info->failures;

    portEXIT_CRITICAL(&pool_info->mutex);
    return true;
}

bool subghz_memory_get_total_stats(SubGhzMemoryStats *stats) {
    if (!stats || !memory_system_initialized) { return false; }

    memset(stats, 0, sizeof(SubGhzMemoryStats));

    for (int i = 0; i < SUBGHZ_POOL_COUNT; i++) {
        SubGhzMemoryStats pool_stats;
        if (subghz_memory_get_stats((SubGhzMemoryPool)i, &pool_stats)) {
            stats->total_allocated += pool_stats.total_allocated;
            stats->total_freed += pool_stats.total_freed;
            stats->current_usage += pool_stats.current_usage;
            stats->peak_usage += pool_stats.peak_usage;
            stats->allocation_count += pool_stats.allocation_count;
            stats->free_count += pool_stats.free_count;
            stats->allocation_failures += pool_stats.allocation_failures;
        }
    }

    return true;
}

bool subghz_memory_check_integrity(SubGhzMemoryPool pool) {
    if (pool == SUBGHZ_POOL_COUNT) {
        // Check all pools
        for (int i = 0; i < SUBGHZ_POOL_COUNT; i++) {
            if (!subghz_memory_check_integrity((SubGhzMemoryPool)i)) { return false; }
        }
        return true;
    }

    if (!validate_pool(pool)) { return false; }

    // For linear allocator, basic integrity check
    SubGhzMemoryPoolInfo *pool_info = &memory_pools[pool];
    return pool_info->used_size <= pool_info->total_size;
}

void subghz_memory_gc(SubGhzMemoryPool pool) {
    // TODO: Implement proper memory garbage collection and defragmentation
    if (pool == SUBGHZ_POOL_COUNT) {
        ESP_LOGI(TAG, "Running GC on all pools");
        for (int i = 0; i < SUBGHZ_POOL_COUNT; i++) { subghz_memory_gc((SubGhzMemoryPool)i); }
    } else if (validate_pool(pool)) {
        ESP_LOGI(TAG, "Running GC on %s pool", pool_names[pool]);

        SubGhzMemoryPoolInfo *pool_info = &memory_pools[pool];
        if (!pool_info->initialized) { return; }

        portENTER_CRITICAL(&pool_info->mutex);

        // Simple memory pool optimization
        // Since we use heap_caps_malloc, we rely on ESP32 heap management
        // This implementation could be enhanced with custom block tracking

        // For now, just reset allocation counters and check integrity
        ESP_LOGI(
            TAG,
            "Pool %s: Used %d/%d bytes, %d allocations, %d failures",
            pool_names[pool],
            pool_info->used_size,
            pool_info->total_size,
            pool_info->allocation_count,
            pool_info->failures
        );

        // Reset peak usage tracking (optional optimization)
        pool_info->peak_usage = pool_info->used_size;

        portEXIT_CRITICAL(&pool_info->mutex);

        ESP_LOGI(TAG, "GC completed for %s pool", pool_names[pool]);
    }
}

size_t subghz_memory_get_available(SubGhzMemoryPool pool) {
    if (!validate_pool(pool)) { return 0; }

    SubGhzMemoryPoolInfo *pool_info = &memory_pools[pool];
    portENTER_CRITICAL(&pool_info->mutex);
    size_t available = pool_info->total_size - pool_info->used_size;
    portEXIT_CRITICAL(&pool_info->mutex);

    return available;
}

void subghz_memory_set_pressure_callback(void (*callback)(SubGhzMemoryPool pool)) {
    pressure_callback = callback;
}

// Debugging functions
void subghz_memory_print_stats(void) {
    if (!memory_system_initialized) {
        ESP_LOGW(TAG, "Memory system not initialized");
        return;
    }

    ESP_LOGI(TAG, "=== SubGHz Memory Statistics ===");

    for (int i = 0; i < SUBGHZ_POOL_COUNT; i++) {
        SubGhzMemoryStats stats;
        if (subghz_memory_get_stats((SubGhzMemoryPool)i, &stats)) {
            ESP_LOGI(TAG, "%s Pool:", pool_names[i]);
            ESP_LOGI(
                TAG,
                "  Current: %zu/%zu bytes (%.1f%%)",
                stats.current_usage,
                pool_sizes[i],
                (float)stats.current_usage * 100.0f / pool_sizes[i]
            );
            ESP_LOGI(
                TAG,
                "  Peak: %zu bytes (%.1f%%)",
                stats.peak_usage,
                (float)stats.peak_usage * 100.0f / pool_sizes[i]
            );
            ESP_LOGI(
                TAG,
                "  Allocations: %u, Frees: %u, Failures: %u",
                stats.allocation_count,
                stats.free_count,
                stats.allocation_failures
            );
        }
    }

    ESP_LOGI(TAG, "===============================");
}
