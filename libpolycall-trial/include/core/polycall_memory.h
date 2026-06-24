#ifndef POLYCALL_MEMORY_H
#define POLYCALL_MEMORY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @file polycall_memory.h
 * @brief Data-oriented memory management system for LibPolyCall
 *
 * This file provides a memory management system designed for data-oriented
 * programming, with focus on cache-friendly memory layouts, batch processing,
 * and efficient memory utilization.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Memory allocation strategy
 */
typedef enum {
    POLYCALL_MEM_STRATEGY_POOL,     /**< Memory pool allocation */
    POLYCALL_MEM_STRATEGY_ARENA,    /**< Arena allocation */
    POLYCALL_MEM_STRATEGY_CHUNKED,  /**< Chunked allocation */
    POLYCALL_MEM_STRATEGY_SLAB      /**< Slab allocation */
} PolycallMemStrategy;

/**
 * @brief Memory allocation flags
 */
typedef enum {
    POLYCALL_MEM_FLAG_NONE       = 0x00,  /**< No flags */
    POLYCALL_MEM_FLAG_ZERO_INIT  = 0x01,  /**< Zero-initialize memory */
    POLYCALL_MEM_FLAG_ALIGN_16   = 0x02,  /**< 16-byte alignment */
    POLYCALL_MEM_FLAG_ALIGN_32   = 0x04,  /**< 32-byte alignment */
    POLYCALL_MEM_FLAG_ALIGN_64   = 0x08,  /**< 64-byte alignment */
    POLYCALL_MEM_FLAG_TEMPORARY  = 0x10,  /**< Temporary allocation */
    POLYCALL_MEM_FLAG_PERSISTENT = 0x20   /**< Persistent allocation */
} PolycallMemFlags;

/**
 * @brief Memory block header
 * Used to track allocations within a memory arena
 */
typedef struct PolycallMemBlock {
    size_t size;                      /**< Block size */
    struct PolycallMemBlock* next;    /**< Next block in chain */
    uint32_t flags;                   /**< Block flags */
    uint32_t magic;                   /**< Magic number for validation */
} PolycallMemBlock;

/**
 * @brief Memory arena
 * Manages a contiguous region of memory with efficient allocation
 */
typedef struct {
    void* base_address;               /**< Base memory address */
    size_t total_size;                /**< Total arena size */
    size_t used_size;                 /**< Used memory size */
    size_t peak_used;                 /**< Peak memory usage */
    PolycallMemBlock* free_list;      /**< List of free blocks */
    PolycallMemStrategy strategy;     /**< Allocation strategy */
    uint32_t flags;                   /**< Arena flags */
    void* user_data;                  /**< User context data */
} PolycallMemArena;

/**
 * @brief Memory pool for fixed-size allocations
 * Optimized for allocating many objects of the same size
 */
typedef struct {
    void* base_address;               /**< Base memory address */
    size_t block_size;                /**< Size of each block */
    size_t num_blocks;                /**< Total number of blocks */
    size_t num_used;                  /**< Number of used blocks */
    uint8_t* free_bitmap;             /**< Bitmap of free blocks */
    uint32_t flags;                   /**< Pool flags */
    void* user_data;                  /**< User context data */
} PolycallMemPool;

/**
 * @brief Memory manager
 * Global memory management system
 */
typedef struct {
    PolycallMemArena** arenas;        /**< Array of memory arenas */
    size_t arena_count;               /**< Number of arenas */
    size_t arena_capacity;            /**< Capacity of arenas array */
    PolycallMemPool** pools;          /**< Array of memory pools */
    size_t pool_count;                /**< Number of pools */
    size_t pool_capacity;             /**< Capacity of pools array */
    size_t total_allocated;           /**< Total allocated memory */
    size_t peak_allocated;            /**< Peak allocated memory */
    void* (*malloc_func)(size_t);     /**< Custom malloc function */
    void (*free_func)(void*);         /**< Custom free function */
    uint32_t flags;                   /**< Manager flags */
    void* user_data;                  /**< User context data */
} PolycallMemManager;

/**
 * @brief Create memory manager
 * 
 * @param malloc_func Custom malloc function (NULL for default)
 * @param free_func Custom free function (NULL for default)
 * @param flags Memory manager flags
 * @return PolycallMemManager* Memory manager or NULL on failure
 */
PolycallMemManager* polycall_mem_create_manager(
    void* (*malloc_func)(size_t),
    void (*free_func)(void*),
    uint32_t flags
);

/**
 * @brief Destroy memory manager
 * 
 * @param manager Memory manager to destroy
 */
void polycall_mem_destroy_manager(PolycallMemManager* manager);

/**
 * @brief Create memory arena
 * 
 * @param manager Memory manager
 * @param size Arena size in bytes
 * @param strategy Allocation strategy
 * @param flags Arena flags
 * @return PolycallMemArena* Memory arena or NULL on failure
 */
PolycallMemArena* polycall_mem_create_arena(
    PolycallMemManager* manager,
    size_t size,
    PolycallMemStrategy strategy,
    uint32_t flags
);

/**
 * @brief Destroy memory arena
 * 
 * @param manager Memory manager
 * @param arena Memory arena to destroy
 */
void polycall_mem_destroy_arena(
    PolycallMemManager* manager,
    PolycallMemArena* arena
);

/**
 * @brief Create memory pool for fixed-size allocations
 * 
 * @param manager Memory manager
 * @param block_size Size of each block
 * @param num_blocks Number of blocks in the pool
 * @param flags Pool flags
 * @return PolycallMemPool* Memory pool or NULL on failure
 */
PolycallMemPool* polycall_mem_create_pool(
    PolycallMemManager* manager,
    size_t block_size,
    size_t num_blocks,
    uint32_t flags
);

/**
 * @brief Destroy memory pool
 * 
 * @param manager Memory manager
 * @param pool Memory pool to destroy
 */
void polycall_mem_destroy_pool(
    PolycallMemManager* manager,
    PolycallMemPool* pool
);

/**
 * @brief Allocate memory from arena
 * 
 * @param arena Memory arena
 * @param size Size in bytes
 * @param flags Allocation flags
 * @return void* Allocated memory or NULL on failure
 */
void* polycall_mem_arena_alloc(
    PolycallMemArena* arena,
    size_t size,
    uint32_t flags
);

/**
 * @brief Free memory in arena
 * 
 * @param arena Memory arena
 * @param ptr Memory pointer
 * @return bool True if successful
 */
bool polycall_mem_arena_free(
    PolycallMemArena* arena,
    void* ptr
);

/**
 * @brief Reset memory arena
 * Quickly resets arena to initial state, invalidating all allocations
 * 
 * @param arena Memory arena
 */
void polycall_mem_arena_reset(PolycallMemArena* arena);

/**
 * @brief Allocate from memory pool
 * 
 * @param pool Memory pool
 * @return void* Allocated block or NULL on failure
 */
void* polycall_mem_pool_alloc(PolycallMemPool* pool);

/**
 * @brief Free block in memory pool
 * 
 * @param pool Memory pool
 * @param ptr Block pointer
 * @return bool True if successful
 */
bool polycall_mem_pool_free(
    PolycallMemPool* pool,
    void* ptr
);

/**
 * @brief Reset memory pool
 * 
 * @param pool Memory pool
 */
void polycall_mem_pool_reset(PolycallMemPool* pool);

/**
 * @brief Batch allocate multiple blocks from pool
 * More efficient than multiple single allocations
 * 
 * @param pool Memory pool
 * @param count Number of blocks to allocate
 * @param out_ptrs Array to receive allocated pointers
 * @return size_t Number of blocks successfully allocated
 */
size_t polycall_mem_pool_batch_alloc(
    PolycallMemPool* pool,
    size_t count,
    void** out_ptrs
);

/**
 * @brief Batch free multiple blocks in pool
 * 
 * @param pool Memory pool
 * @param ptrs Array of pointers to free
 * @param count Number of pointers
 * @return size_t Number of blocks successfully freed
 */
size_t polycall_mem_pool_batch_free(
    PolycallMemPool* pool,
    void** ptrs,
    size_t count
);

/**
 * @brief Get memory manager statistics
 * 
 * @param manager Memory manager
 * @param total_allocated Pointer to receive total allocated memory
 * @param peak_allocated Pointer to receive peak allocated memory
 * @param arena_count Pointer to receive arena count
 * @param pool_count Pointer to receive pool count
 */
void polycall_mem_get_stats(
    const PolycallMemManager* manager,
    size_t* total_allocated,
    size_t* peak_allocated,
    size_t* arena_count,
    size_t* pool_count
);

/**
 * @brief Get memory arena statistics
 * 
 * @param arena Memory arena
 * @param total_size Pointer to receive total size
 * @param used_size Pointer to receive used size
 * @param peak_used Pointer to receive peak usage
 * @param fragmentation Pointer to receive fragmentation percentage
 */
void polycall_mem_arena_get_stats(
    const PolycallMemArena* arena,
    size_t* total_size,
    size_t* used_size,
    size_t* peak_used,
    float* fragmentation
);

/**
 * @brief Get memory pool statistics
 * 
 * @param pool Memory pool
 * @param block_size Pointer to receive block size
 * @param num_blocks Pointer to receive total blocks
 * @param num_used Pointer to receive used blocks
 * @param usage_percent Pointer to receive usage percentage
 */
void polycall_mem_pool_get_stats(
    const PolycallMemPool* pool,
    size_t* block_size,
    size_t* num_blocks,
    size_t* num_used,
    float* usage_percent
);

#ifdef __cplusplus
}
#endif

#endif /* POLYCALL_MEMORY_H */