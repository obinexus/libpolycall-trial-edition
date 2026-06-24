#include "core/polycall_memory.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define POLYCALL_MEMORY_MAGIC 0x504D454D // "PMEM"
#define POLYCALL_ARENA_MIN_SIZE 4096
#define POLYCALL_DEFAULT_ALIGNMENT 8
#define POLYCALL_MIN_BLOCK_SIZE sizeof(PolycallMemBlock)

/* Utility macros and inline functions */

#define ALIGN_UP(value, alignment) \
    ((((value) + (alignment) - 1) / (alignment)) * (alignment))

#define IS_POWER_OF_TWO(x) (((x) != 0) && (((x) & ((x) - 1)) == 0))

static void* default_malloc_func(size_t size) {
    return malloc(size);
}

static void default_free_func(void* ptr) {
    free(ptr);
}

static size_t calculate_alignment(uint32_t flags) {
    if (flags & POLYCALL_MEM_FLAG_ALIGN_64) return 64;
    if (flags & POLYCALL_MEM_FLAG_ALIGN_32) return 32;
    if (flags & POLYCALL_MEM_FLAG_ALIGN_16) return 16;
    return POLYCALL_DEFAULT_ALIGNMENT;
}

/* Memory Manager Implementation */

PolycallMemManager* polycall_mem_create_manager(
    void* (*malloc_func)(size_t),
    void (*free_func)(void*),
    uint32_t flags
) {
    // Use default functions if not provided
    if (!malloc_func) malloc_func = default_malloc_func;
    if (!free_func) free_func = default_free_func;
    
    // Allocate manager
    PolycallMemManager* manager = malloc_func(sizeof(PolycallMemManager));
    if (!manager) return NULL;
    
    // Initialize manager
    memset(manager, 0, sizeof(PolycallMemManager));
    manager->malloc_func = malloc_func;
    manager->free_func = free_func;
    manager->flags = flags;
    
    // Allocate initial arrays
    manager->arena_capacity = 4;
    manager->pool_capacity = 4;
    manager->arenas = malloc_func(manager->arena_capacity * sizeof(PolycallMemArena*));
    manager->pools = malloc_func(manager->pool_capacity * sizeof(PolycallMemPool*));
    
    if (!manager->arenas || !manager->pools) {
        if (manager->arenas) free_func(manager->arenas);
        if (manager->pools) free_func(manager->pools);
        free_func(manager);
        return NULL;
    }
    
    return manager;
}

void polycall_mem_destroy_manager(PolycallMemManager* manager) {
    if (!manager) return;
    
    // Free all arenas
    for (size_t i = 0; i < manager->arena_count; i++) {
        polycall_mem_destroy_arena(manager, manager->arenas[i]);
    }
    
    // Free all pools
    for (size_t i = 0; i < manager->pool_count; i++) {
        polycall_mem_destroy_pool(manager, manager->pools[i]);
    }
    
    // Free arrays
    manager->free_func(manager->arenas);
    manager->free_func(manager->pools);
    
    // Free manager
    manager->free_func(manager);
}

/* Memory Arena Implementation */

PolycallMemArena* polycall_mem_create_arena(
    PolycallMemManager* manager,
    size_t size,
    PolycallMemStrategy strategy,
    uint32_t flags
) {
    if (!manager || size < POLYCALL_ARENA_MIN_SIZE) return NULL;
    
    // Ensure we have capacity in the manager
    if (manager->arena_count >= manager->arena_capacity) {
        size_t new_capacity = manager->arena_capacity * 2;
        PolycallMemArena** new_arenas = manager->malloc_func(
            new_capacity * sizeof(PolycallMemArena*)
        );
        
        if (!new_arenas) return NULL;
        
        memcpy(new_arenas, manager->arenas, 
               manager->arena_count * sizeof(PolycallMemArena*));
        manager->free_func(manager->arenas);
        manager->arenas = new_arenas;
        manager->arena_capacity = new_capacity;
    }
    
    // Allocate arena
    PolycallMemArena* arena = manager->malloc_func(sizeof(PolycallMemArena));
    if (!arena) return NULL;
    
    // Initialize arena
    memset(arena, 0, sizeof(PolycallMemArena));
    arena->total_size = size;
    arena->strategy = strategy;
    arena->flags = flags;
    
    // Allocate memory for arena
    arena->base_address = manager->malloc_func(size);
    if (!arena->base_address) {
        manager->free_func(arena);
        return NULL;
    }
    
    // Initialize free list (for POOL and CHUNKED strategies)
    if (strategy == POLYCALL_MEM_STRATEGY_POOL || 
        strategy == POLYCALL_MEM_STRATEGY_CHUNKED) {
        // Initialize as one large free block
        PolycallMemBlock* block = (PolycallMemBlock*)arena->base_address;
        block->size = size - sizeof(PolycallMemBlock);
        block->next = NULL;
        block->flags = 0;
        block->magic = POLYCALL_MEMORY_MAGIC;
        arena->free_list = block;
    }
    
    // Add to manager
    manager->arenas[manager->arena_count++] = arena;
    manager->total_allocated += size;
    
    if (manager->total_allocated > manager->peak_allocated) {
        manager->peak_allocated = manager->total_allocated;
    }
    
    return arena;
}

void polycall_mem_destroy_arena(
    PolycallMemManager* manager,
    PolycallMemArena* arena
) {
    if (!manager || !arena) return;
    
    // Remove from manager's list
    bool found = false;
    for (size_t i = 0; i < manager->arena_count; i++) {
        if (manager->arenas[i] == arena) {
            // Shift remaining arenas down
            memmove(&manager->arenas[i], &manager->arenas[i + 1], 
                    (manager->arena_count - i - 1) * sizeof(PolycallMemArena*));
            manager->arena_count--;
            found = true;
            break;
        }
    }
    
    if (!found) return; // Arena not managed by this manager
    
    // Update manager stats
    manager->total_allocated -= arena->total_size;
    
    // Free arena memory
    manager->free_func(arena->base_address);
    manager->free_func(arena);
}

void* polycall_mem_arena_alloc(
    PolycallMemArena* arena,
    size_t size,
    uint32_t flags
) {
    if (!arena || !size) return NULL;
    
    void* result = NULL;
    size_t alignment = calculate_alignment(flags);
    
    // Align size to required alignment
    size = ALIGN_UP(size, alignment);
    
    // Ensure minimum block size
    if (size < POLYCALL_MIN_BLOCK_SIZE) {
        size = POLYCALL_MIN_BLOCK_SIZE;
    }
    
    // Allocation strategy
    switch (arena->strategy) {
        case POLYCALL_MEM_STRATEGY_ARENA:
            // Simple linear allocation
            if (arena->used_size + size <= arena->total_size) {
                result = (uint8_t*)arena->base_address + arena->used_size;
                arena->used_size += size;
                
                if (arena->used_size > arena->peak_used) {
                    arena->peak_used = arena->used_size;
                }
            }
            break;
            
        case POLYCALL_MEM_STRATEGY_POOL:
        case POLYCALL_MEM_STRATEGY_CHUNKED: {
            // Find a free block that can fit the allocation
            PolycallMemBlock* prev = NULL;
            PolycallMemBlock* block = arena->free_list;
            
            while (block) {
                if (block->size >= size) {
                    // Found a block large enough
                    
                    // Check if we should split
                    if (block->size > size + sizeof(PolycallMemBlock) + POLYCALL_MIN_BLOCK_SIZE) {
                        // Split block
                        PolycallMemBlock* new_block = (PolycallMemBlock*)(
                            (uint8_t*)block + sizeof(PolycallMemBlock) + size
                        );
                        
                        new_block->size = block->size - size - sizeof(PolycallMemBlock);
                        new_block->next = block->next;
                        new_block->flags = 0;
                        new_block->magic = POLYCALL_MEMORY_MAGIC;
                        
                        block->size = size;
                        block->next = new_block;
                    }
                    
                    // Remove from free list
                    if (prev) {
                        prev->next = block->next;
                    } else {
                        arena->free_list = block->next;
                    }
                    
                    // Mark as used
                    block->flags |= POLYCALL_MEM_FLAG_PERSISTENT;
                    
                    // Return memory after header
                    result = (uint8_t*)block + sizeof(PolycallMemBlock);
                    arena->used_size += block->size + sizeof(PolycallMemBlock);
                    
                    if (arena->used_size > arena->peak_used) {
                        arena->peak_used = arena->used_size;
                    }
                    
                    break;
                }
                
                prev = block;
                block = block->next;
            }
            break;
        }
            
        case POLYCALL_MEM_STRATEGY_SLAB:
            // Not implemented for this example
            break;
    }
    
    // Zero-initialize if requested
    if (result && (flags & POLYCALL_MEM_FLAG_ZERO_INIT)) {
        memset(result, 0, size);
    }
    
    return result;
}

bool polycall_mem_arena_free(
    PolycallMemArena* arena,
    void* ptr
) {
    if (!arena || !ptr) return false;
    
    // Can't free memory in arenas that don't track allocations
    if (arena->strategy == POLYCALL_MEM_STRATEGY_ARENA) {
        return false;
    }
    
    // Get block header
    PolycallMemBlock* block = (PolycallMemBlock*)((uint8_t*)ptr - sizeof(PolycallMemBlock));
    
    // Validate block
    if (block->magic != POLYCALL_MEMORY_MAGIC) {
        return false;
    }
    
    // Add to free list
    block->next = arena->free_list;
    block->flags &= ~POLYCALL_MEM_FLAG_PERSISTENT;
    arena->free_list = block;
    
    // Update used size
    arena->used_size -= (block->size + sizeof(PolycallMemBlock));
    
    return true;
}

void polycall_mem_arena_reset(PolycallMemArena* arena) {
    if (!arena) return;
    
    // Reset usage
    arena->used_size = 0;
    
    // Strategy-specific reset
    switch (arena->strategy) {
        case POLYCALL_MEM_STRATEGY_ARENA:
            // Nothing to do
            break;
            
        case POLYCALL_MEM_STRATEGY_POOL:
        case POLYCALL_MEM_STRATEGY_CHUNKED:
            // Clear all memory first
            memset(arena->base_address, 0, arena->total_size);
            // Recreate single free block
            PolycallMemBlock* block = (PolycallMemBlock*)arena->base_address;
            block->size = arena->total_size - sizeof(PolycallMemBlock);
            block->next = NULL;
            block->flags = 0;
            block->magic = POLYCALL_MEMORY_MAGIC;
            arena->free_list = block;
            break;
            
        case POLYCALL_MEM_STRATEGY_SLAB:
            // Not implemented for this example
            break;
    }
}

/* Memory Pool Implementation */

PolycallMemPool* polycall_mem_create_pool(
    PolycallMemManager* manager,
    size_t block_size,
    size_t num_blocks,
    uint32_t flags
) {
    if (!manager || block_size == 0 || num_blocks == 0) return NULL;
    
    // Ensure we have capacity in the manager
    if (manager->pool_count >= manager->pool_capacity) {
        size_t new_capacity = manager->pool_capacity * 2;
        PolycallMemPool** new_pools = manager->malloc_func(
            new_capacity * sizeof(PolycallMemPool*)
        );
        
        if (!new_pools) return NULL;
        
        memcpy(new_pools, manager->pools, 
               manager->pool_count * sizeof(PolycallMemPool*));
        manager->free_func(manager->pools);
        manager->pools = new_pools;
        manager->pool_capacity = new_capacity;
    }
    
    // Allocate pool
    PolycallMemPool* pool = manager->malloc_func(sizeof(PolycallMemPool));
    if (!pool) return NULL;
    
    // Initialize pool
    memset(pool, 0, sizeof(PolycallMemPool));
    pool->block_size = block_size;
    pool->num_blocks = num_blocks;
    pool->flags = flags;
    
    // Calculate bitmap size (1 bit per block)
    size_t bitmap_size = (num_blocks + 7) / 8; // Round up to bytes
    
    // Calculate total memory size
    size_t total_size = num_blocks * block_size;
    
    // Allocate memory for pool
    pool->base_address = manager->malloc_func(total_size);
    pool->free_bitmap = manager->malloc_func(bitmap_size);
    
    if (!pool->base_address || !pool->free_bitmap) {
        if (pool->base_address) manager->free_func(pool->base_address);
        if (pool->free_bitmap) manager->free_func(pool->free_bitmap);
        manager->free_func(pool);
        return NULL;
    }
    
    // Initialize bitmap (all blocks free)
    memset(pool->free_bitmap, 0, bitmap_size);
    
    // Add to manager
    manager->pools[manager->pool_count++] = pool;
    manager->total_allocated += total_size;
    
    if (manager->total_allocated > manager->peak_allocated) {
        manager->peak_allocated = manager->total_allocated;
    }
    
    return pool;
}

void polycall_mem_destroy_pool(
    PolycallMemManager* manager,
    PolycallMemPool* pool
) {
    if (!manager || !pool) return;
    
    // Remove from manager's list
    bool found = false;
    for (size_t i = 0; i < manager->pool_count; i++) {
        if (manager->pools[i] == pool) {
            // Shift remaining pools down
            memmove(&manager->pools[i], &manager->pools[i + 1], 
                    (manager->pool_count - i - 1) * sizeof(PolycallMemPool*));
            manager->pool_count--;
            found = true;
            break;
        }
    }
    
    if (!found) return; // Pool not managed by this manager
    
    // Update manager stats
    manager->total_allocated -= pool->num_blocks * pool->block_size;
    
    // Free pool memory
    manager->free_func(pool->base_address);
    manager->free_func(pool->free_bitmap);
    manager->free_func(pool);
}

void* polycall_mem_pool_alloc(PolycallMemPool* pool) {
    if (!pool) return NULL;
    
    // Find a free block
    for (size_t i = 0; i < pool->num_blocks; i++) {
        // Check if block is free
        size_t byte_index = i / 8;
        uint8_t bit_mask = 1 << (i % 8);
        
        if (!(pool->free_bitmap[byte_index] & bit_mask)) {
            // Block is free, mark as used
            pool->free_bitmap[byte_index] |= bit_mask;
            pool->num_used++;
            
            // Return block
            return (uint8_t*)pool->base_address + (i * pool->block_size);
        }
    }
    
    // No free blocks
    return NULL;
}

bool polycall_mem_pool_free(
    PolycallMemPool* pool,
    void* ptr
) {
    if (!pool || !ptr) return false;
    
    // Calculate block index
    ptrdiff_t offset = (uint8_t*)ptr - (uint8_t*)pool->base_address;
    if (offset < 0 || offset >= (ptrdiff_t)(pool->num_blocks * pool->block_size)) {
        return false; // Not in pool
    }
    
    size_t block_index = offset / pool->block_size;
    if (offset % pool->block_size != 0) {
        return false; // Not aligned to block boundary
    }
    
    // Check if block is already free
    size_t byte_index = block_index / 8;
    uint8_t bit_mask = 1 << (block_index % 8);
    
    if (!(pool->free_bitmap[byte_index] & bit_mask)) {
        return false; // Already free
    }
    
    // Mark as free
    pool->free_bitmap[byte_index] &= ~bit_mask;
    pool->num_used--;
    
    return true;
}

void polycall_mem_pool_reset(PolycallMemPool* pool) {
    if (!pool) return;
    
    // Mark all blocks as free
    size_t bitmap_size = (pool->num_blocks + 7) / 8;
    memset(pool->free_bitmap, 0, bitmap_size);
    pool->num_used = 0;
}

size_t polycall_mem_pool_batch_alloc(
    PolycallMemPool* pool,
    size_t count,
    void** out_ptrs
) {
    if (!pool || !out_ptrs || count == 0) return 0;
    
    size_t allocated = 0;
    
    // Find free blocks
    for (size_t i = 0; i < pool->num_blocks && allocated < count; i++) {
        // Check if block is free
        size_t byte_index = i / 8;
        uint8_t bit_mask = 1 << (i % 8);
        
        if (!(pool->free_bitmap[byte_index] & bit_mask)) {
            // Block is free, mark as used
            pool->free_bitmap[byte_index] |= bit_mask;
            
            // Return block
            out_ptrs[allocated] = (uint8_t*)pool->base_address + (i * pool->block_size);
            allocated++;
        }
    }
    
    pool->num_used += allocated;
    return allocated;
}

size_t polycall_mem_pool_batch_free(
    PolycallMemPool* pool,
    void** ptrs,
    size_t count
) {
    if (!pool || !ptrs || count == 0) return 0;
    
    size_t freed = 0;
    
    // Process each pointer
    for (size_t i = 0; i < count; i++) {
        // Skip NULL pointers
        if (!ptrs[i]) continue;
        
        // Calculate block index
        ptrdiff_t offset = (uint8_t*)ptrs[i] - (uint8_t*)pool->base_address;
        if (offset < 0 || offset >= (ptrdiff_t)(pool->num_blocks * pool->block_size)) {
            continue; // Not in pool
        }
        
        size_t block_index = offset / pool->block_size;
        if (offset % pool->block_size != 0) {
            continue; // Not aligned to block boundary
        }
        
        // Check if block is already free
        size_t byte_index = block_index / 8;
        uint8_t bit_mask = 1 << (block_index % 8);
        
        if (pool->free_bitmap[byte_index] & bit_mask) {
            // Mark as free
            pool->free_bitmap[byte_index] &= ~bit_mask;
            freed++;
        }
    }
    
    pool->num_used -= freed;
    return freed;
}

/* Statistics functions */

void polycall_mem_get_stats(
    const PolycallMemManager* manager,
    size_t* total_allocated,
    size_t* peak_allocated,
    size_t* arena_count,
    size_t* pool_count
) {
    if (!manager) return;
    
    if (total_allocated) *total_allocated = manager->total_allocated;
    if (peak_allocated) *peak_allocated = manager->peak_allocated;
    if (arena_count) *arena_count = manager->arena_count;
    if (pool_count) *pool_count = manager->pool_count;
}

void polycall_mem_arena_get_stats(
    const PolycallMemArena* arena,
    size_t* total_size,
    size_t* used_size,
    size_t* peak_used,
    float* fragmentation
) {
    if (!arena) return;
    
    if (total_size) *total_size = arena->total_size;
    if (used_size) *used_size = arena->used_size;
    if (peak_used) *peak_used = arena->peak_used;
    
    // Calculate fragmentation
    if (fragmentation) {
        if (arena->strategy == POLYCALL_MEM_STRATEGY_ARENA) {
            // Linear allocator has no fragmentation
            *fragmentation = 0.0f;
        } else if (arena->strategy == POLYCALL_MEM_STRATEGY_POOL ||
                  arena->strategy == POLYCALL_MEM_STRATEGY_CHUNKED) {
            // Count free blocks
            size_t free_blocks = 0;
            PolycallMemBlock* block = arena->free_list;
            
            while (block) {
                free_blocks++;
                block = block->next;
            }
            
            if (free_blocks <= 1) {
                *fragmentation = 0.0f;
            } else {
                // Fragmentation increases with number of free blocks
                *fragmentation = (float)(free_blocks - 1) / 
                                (float)(arena->total_size / POLYCALL_MIN_BLOCK_SIZE);
                
                // Cap at 1.0
                if (*fragmentation > 1.0f) *fragmentation = 1.0f;
            }
        } else {
            *fragmentation = 0.0f;
        }
    }
}

void polycall_mem_pool_get_stats(
    const PolycallMemPool* pool,
    size_t* block_size,
    size_t* num_blocks,
    size_t* num_used,
    float* usage_percent
) {
    if (!pool) return;
    
    if (block_size) *block_size = pool->block_size;
    if (num_blocks) *num_blocks = pool->num_blocks;
    if (num_used) *num_used = pool->num_used;
    
    if (usage_percent) {
        *usage_percent = pool->num_blocks > 0 ? 
            (float)pool->num_used / (float)pool->num_blocks * 100.0f : 0.0f;
    }
}