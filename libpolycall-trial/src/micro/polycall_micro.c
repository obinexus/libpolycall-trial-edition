
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "micro/polycall_micro.h"
#include "core/polycall.h"
#include "core/polycall_memory.h"
#include "core/polycall_utils.h"
#include "protocol/polycall_protocol.h"
#include "network/polycall_network.h"
#include "state/polycall_state_machine.h"
#include "crypto/polycall_crypto.h" 





// Global operation state for composition
static PolycallOperation g_first_operation;
static PolycallOperation g_second_operation;

// Define PredicateContext structure
typedef struct {
    PolycallPredicate first;
    PolycallPredicate second;
} PredicateContext;

// Global context for predicate composition
static PredicateContext g_predicate_and_context;
static PredicateContext g_predicate_or_context;

// Internal implementation of predicate composition
static bool predicate_and_impl(const PolycallCommand* cmd) {
    PredicateContext* ctx = &g_predicate_and_context;
    return ctx->first(cmd) && ctx->second(cmd);
}

static bool predicate_or_impl(const PolycallCommand* cmd) {
    PredicateContext* ctx = &g_predicate_or_context;
    return ctx->first(cmd) || ctx->second(cmd);
}

PolycallPredicate polycall_micro_compose_predicates_and(
    PolycallPredicate first,
    PolycallPredicate second
) {
    g_predicate_and_context.first = first;
    g_predicate_and_context.second = second;
    return predicate_and_impl;
}

PolycallPredicate polycall_micro_compose_predicates_or(
    PolycallPredicate first,
    PolycallPredicate second
) {
    g_predicate_or_context.first = first;
    g_predicate_or_context.second = second;
    return predicate_or_impl;
}
// Helper function implementations
static uint64_t get_current_timestamp(void) {
    return (uint64_t)time(NULL);
}

static uint32_t calculate_checksum(const void* data, size_t size) {
    return polycall_crypto_calculate_checksum(data, size, POLYCALL_CRYPTO_ALG_SHA256);
}

void reset_service_state(PolycallServiceState* service) {
    if (!service) return;
    
    // Save service ID if we're resetting an existing service
    uint32_t id = service->id;
    char service_name[32];
    memcpy(service_name, service->service_name, sizeof(service_name));
    
    // Clear entire service state in a single operation
    memset(service, 0, sizeof(PolycallServiceState));
    
    // Restore ID and name
    service->id = id;
    memcpy(service->service_name, service_name, sizeof(service->service_name));
    
    // Reinitialize command queue
    service->command_queue.count = 0;
    service->command_queue.capacity = POLYCALL_MICRO_MAX_COMMANDS;
}

static void update_service_checksum(PolycallServiceState* service) {
    if (!service) return;
    
    service->checksum = polycall_crypto_calculate_checksum(
        service, 
        offsetof(PolycallServiceState, checksum),
        POLYCALL_CRYPTO_ALG_SHA256
    );
}

// Implement inline utility functions if not already defined in headers
static bool verify_service_integrity(const PolycallServiceState* service) {
    if (!service) return false;
    
    // Perform basic integrity checks
    // Check memory guards
    if (service->guard_pre.magic != POLYCALL_MEMORY_GUARD_MAGIC ||
        service->guard_post.magic != POLYCALL_MEMORY_GUARD_MAGIC) {
        return false;
    }
    
    // Verify checksum
    uint32_t calculated_checksum = polycall_crypto_calculate_checksum(
        service, 
        offsetof(PolycallServiceState, checksum),
        POLYCALL_CRYPTO_ALG_SHA256
    );
    
    return service->checksum == calculated_checksum;
}
/**
 * @brief Process service before cleanup
 */
PolycallMicroStatus polycall_micro_process_service(
    PolycallServiceState* service,
    PolycallOperation operation
) {
    if (!service || !operation) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Verify service integrity before processing
    if (!verify_service_integrity(service)) {
        return POLYCALL_MICRO_ERROR_INTEGRITY;
    }
    
    // Apply operation (point-free style)
    operation(service);
    
    // Update timestamp
    service->last_update = get_current_timestamp();
    
    // Update service checksum after operation
    update_service_checksum(service);
    
    return POLYCALL_MICRO_SUCCESS;
}

/**
 * @brief Batch process commands for service
 */
PolycallMicroStatus polycall_micro_batch_process(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    const PolycallCommandArray* commands
) {
    if (!ctx || service_id == 0 || !commands) {
        return POLYCALL_MICRO_ERROR_COMMAND;
    }
    
    // Find service
    PolycallServiceState* service = find_service_by_id(ctx, service_id);
    if (!service) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Process commands in batch for efficiency (data-oriented approach)
    uint32_t commands_processed = 0;
    uint32_t queue_space = service->command_queue.capacity - service->command_queue.count;
    
    // Determine how many commands we can process
    uint32_t process_count = (commands->count < queue_space) ? commands->count : queue_space;
    
    if (process_count == 0) {
        // Queue is full
        return POLYCALL_MICRO_SUCCESS; // Return success but process nothing
    }
    
    // Process commands in bulk
    for (uint32_t i = 0; i < process_count; i++) {
        const PolycallCommand* cmd = &commands->commands[i];
        
        // Verify command is valid for this service
        if (cmd->target_service_id != 0 && cmd->target_service_id != service_id) {
            continue; // Skip commands not intended for this service
        }
        
        // Verify command has not expired
        if (cmd->expiration < get_current_timestamp()) {
            continue; // Skip expired commands
        }
        
        // Verify command integrity
        uint32_t cmd_checksum = calculate_checksum(cmd, offsetof(PolycallCommand, signature));
        if (cmd_checksum != cmd->checksum) {
            continue; // Skip commands with invalid checksum
        }
        
        // Add to service command queue
        memcpy(&service->command_queue.commands[service->command_queue.count],
               cmd, sizeof(PolycallCommand));
        
        service->command_queue.count++;
        commands_processed++;
    }
    
    // Update command queue checksum
    service->command_queue.checksum = calculate_checksum(
        &service->command_queue, 
        offsetof(PolycallCommandArray, checksum)
    );
    
    // Update service timestamp
    service->last_update = get_current_timestamp();
    
    // Update service checksum
    update_service_checksum(service);
    
    return POLYCALL_MICRO_SUCCESS;
}

/**
 * @brief Execute command securely
 */
PolycallMicroStatus polycall_micro_execute_command(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    const PolycallCommand* cmd
) {
    if (!ctx || service_id == 0 || !cmd) {
        return POLYCALL_MICRO_ERROR_COMMAND;
    }
    
    // Find service
    PolycallServiceState* service = find_service_by_id(ctx, service_id);
    if (!service) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Verify command integrity
    uint32_t cmd_checksum = calculate_checksum(cmd, offsetof(PolycallCommand, signature));
    if (cmd_checksum != cmd->checksum) {
        return POLYCALL_MICRO_ERROR_INTEGRITY;
    }
    
    // Verify command has not expired
    if (cmd->expiration < get_current_timestamp()) {
        return POLYCALL_MICRO_ERROR_COMMAND;
    }
    
    // Verify command is intended for this service
    if (cmd->target_service_id != 0 && cmd->target_service_id != service_id) {
        return POLYCALL_MICRO_ERROR_PERMISSION;
    }
    
    // Verify source service is allowed to send commands to this service
    if (cmd->service_id != 0 && cmd->service_id != service_id) {
        // Check if source service is in allowed services mask
        if (!(service->security.allowed_service_mask & (1U << (cmd->service_id & 31)))) {
            return POLYCALL_MICRO_ERROR_PERMISSION;
        }
    }
    
    // For zero-trust verification, verify command signature
    if (service->flags & POLYCALL_MICRO_FLAG_ZERO_TRUST) {
        PolycallMicroStatus verify_status = polycall_micro_verify_command(ctx, cmd);
        if (verify_status != POLYCALL_MICRO_SUCCESS) {
            return verify_status;
        }
    }
    
    // Update service timestamp
    service->last_update = get_current_timestamp();
    
    // Update service checksum
    update_service_checksum(service);
    
    return POLYCALL_MICRO_SUCCESS;
}

/**
 * @brief Create transformation chain
 */
PolycallTransformChain polycall_micro_create_transform_chain(
    PolycallTransform* transforms,
    uint32_t count
) {
    PolycallTransformChain chain = {0};
    
    if (transforms && count > 0) {
        chain.transforms = transforms;
        chain.transform_count = count;
        
        // Calculate chain checksum for integrity verification
        chain.checksum = calculate_checksum(transforms, count * sizeof(PolycallTransform));
    }
    
    return chain;
}

/**
 * @brief Destroy transformation chain
 */
void polycall_micro_destroy_transform_chain(
    PolycallTransformChain* chain
) {
    if (chain) {
        chain->transforms = NULL;
        chain->transform_count = 0;
        chain->checksum = 0;
    }
}



/**
 * @brief Compose operation functions
 */
// Function declaration moved outside
static void composed_operation(PolycallServiceState* service);

PolycallOperation polycall_micro_compose_operations(
    PolycallOperation first,
    PolycallOperation second
) {
    
    // Global state to hold operations (not thread-safe - for illustration only)
    extern PolycallOperation g_first_operation;
    extern PolycallOperation g_second_operation;
    
    g_first_operation = first;
    g_second_operation = second;
    
    return composed_operation;
}

/**
 * @brief Process services with operation
 */
PolycallMicroStatus polycall_micro_process_services(
    PolycallMicroContext* ctx,
    PolycallOperation operation,
    PolycallServicePredicate predicate
) {
    if (!ctx || !operation) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    PolycallMicroStatus status = POLYCALL_MICRO_SUCCESS;
    
    // Apply operation to all matching services (data-oriented batch processing)
    for (uint32_t i = 0; i < POLYCALL_MICRO_MAX_SERVICES; i++) {
        if (!(ctx->service_array.active_mask & (1U << i))) {
            continue; // Skip inactive services
        }
        
        PolycallServiceState* service = &ctx->service_array.services[i];
        
        // Skip services that don't match predicate
        if (predicate && !predicate(service)) {
            continue;
        }
        
        // Apply operation to service
        PolycallMicroStatus svc_status = polycall_micro_process_service(service, operation);
        if (svc_status != POLYCALL_MICRO_SUCCESS) {
            status = svc_status; // Report last error
        }
    }
    
    return status;
}


// Function definition moved here
static void composed_operation(PolycallServiceState* service) {
    g_first_operation(service);
    g_second_operation(service);
}
PolycallMicroStatus polycall_micro_generate_keys(
    PolycallMicroContext* ctx,
    uint32_t service_id
) {
    if (!ctx || service_id == 0) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Find service
    PolycallServiceState* service = find_service_by_id(ctx, service_id);
    if (!service) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Generate new keys using crypto library's secure random byte generation
    PolycallCryptoStatus crypto_status;
    
    crypto_status = polycall_crypto_generate_random_bytes(
        service->security.encryption_key, 
        POLYCALL_MICRO_KEY_LENGTH, 
        POLYCALL_CRYPTO_RNG_SYSTEM
    );
    
    if (crypto_status != POLYCALL_CRYPTO_SUCCESS) {
        return POLYCALL_MICRO_ERROR_ENCRYPTION;
    }
    
    crypto_status = polycall_crypto_generate_random_bytes(
        service->security.hmac_key, 
        POLYCALL_MICRO_KEY_LENGTH, 
        POLYCALL_CRYPTO_RNG_SYSTEM
    );
    
    if (crypto_status != POLYCALL_CRYPTO_SUCCESS) {
        return POLYCALL_MICRO_ERROR_ENCRYPTION;
    }
    
    // Update key generation time
    service->security.key_generation_time = get_current_timestamp();
    
    // Update security checksum
    service->security.checksum = polycall_crypto_calculate_checksum(
        &service->security, 
        offsetof(PolycallSecurityContext, checksum),
        POLYCALL_CRYPTO_ALG_SHA256
    );
    
    // Update timestamp
    service->last_update = get_current_timestamp();
    
    // Update service checksum
    update_service_checksum(service);
    
    return POLYCALL_MICRO_SUCCESS;
}

/**
 * @brief Rotate security keys for service
 */
PolycallMicroStatus polycall_micro_rotate_keys(
    PolycallMicroContext* ctx,
    uint32_t service_id
) {
    if (!ctx || service_id == 0) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Find service
    PolycallServiceState* service = find_service_by_id(ctx, service_id);
    if (!service) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Check if key rotation is needed
    uint64_t current_time = get_current_timestamp();
    if (current_time - service->security.key_generation_time < service->security.key_rotation_interval) {
        // Keys are still valid
        return POLYCALL_MICRO_SUCCESS;
    }
    
    // Generate new keys
    return polycall_micro_generate_keys(ctx, service_id);
}

/**
 * @brief Set access control for service
 */
PolycallMicroStatus polycall_micro_set_access_control(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    uint32_t access_mask
) {
    if (!ctx || service_id == 0) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Find service
    PolycallServiceState* service = find_service_by_id(ctx, service_id);
    if (!service) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Update access control mask
    service->security.access_control_mask = access_mask;
    
    // Update security checksum
    service->security.checksum = calculate_checksum(&service->security, 
                                                  offsetof(PolycallSecurityContext, checksum));
    
    // Update timestamp
    service->last_update = get_current_timestamp();
    
    // Update service checksum
    update_service_checksum(service);
    
    return POLYCALL_MICRO_SUCCESS;
}

/**
 * @brief Verify command signature
 */
PolycallMicroStatus polycall_micro_verify_command(
    PolycallMicroContext* ctx,
    const PolycallCommand* cmd
) {
    if (!ctx || !cmd) {
        return POLYCALL_MICRO_ERROR_COMMAND;
    }
    
    // No signature verification for local commands
    if (cmd->service_id == 0) {
        return POLYCALL_MICRO_SUCCESS;
    }
    
    // Find source service
    PolycallServiceState* service = find_service_by_id(ctx, cmd->service_id);
    if (!service) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Verify command checksum
    uint32_t cmd_checksum = calculate_checksum(cmd, offsetof(PolycallCommand, signature));
    if (cmd_checksum != cmd->checksum) {
        return POLYCALL_MICRO_ERROR_INTEGRITY;
    }
    
    // In a real implementation, verify HMAC signature using service's HMAC key
    // For simplicity, we'll just compare the first few bytes as a demo
    const uint8_t* hmac_key = service->security.hmac_key;
    for (uint32_t i = 0; i < 4; i++) {
        if (cmd->signature[i] != hmac_key[i]) {
            return POLYCALL_MICRO_ERROR_INTEGRITY;
        }
    }
    
    return POLYCALL_MICRO_SUCCESS;
}

/**
 * @brief Sign command with service key
 */
PolycallMicroStatus polycall_micro_sign_command(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    PolycallCommand* cmd
) {
    if (!ctx || service_id == 0 || !cmd) {
        return POLYCALL_MICRO_ERROR_COMMAND;
    }
    
    // Find service
    PolycallServiceState* service = find_service_by_id(ctx, service_id);
    if (!service) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Ensure command belongs to service
    cmd->service_id = service_id;
    
    // Update command checksum before signing
    cmd->checksum = calculate_checksum(cmd, offsetof(PolycallCommand, signature));
    
    // In a real implementation, calculate HMAC signature using service's HMAC key
    // For simplicity, we'll just copy the first few bytes of the HMAC key as a demo
    const uint8_t* hmac_key = service->security.hmac_key;
    memcpy(cmd->signature, hmac_key, POLYCALL_MICRO_KEY_LENGTH);
    
    return POLYCALL_MICRO_SUCCESS;
}

/**
 * @brief Allocate memory from service pool
 */
void* polycall_micro_service_alloc(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    size_t size
) {
    if (!ctx || service_id == 0 || size == 0 || size > POLYCALL_MICRO_BUFFER_SIZE) {
        return NULL;
    }
    
    // Find service
    PolycallServiceState* service = find_service_by_id(ctx, service_id);
    if (!service) {
        return NULL;
    }
    
    // In a real implementation, we would have a proper memory allocator
    // For simplicity, we'll just return the start of the memory pool
    // This is not a real implementation, just a demonstration
    return service->memory_pool;
}

/**
 * @brief Free memory from service pool
 */
PolycallMicroStatus polycall_micro_service_free(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    void* ptr
) {
    if (!ctx || service_id == 0 || !ptr) {
        return POLYCALL_MICRO_ERROR_MEMORY;
    }
    
    // Find service
    PolycallServiceState* service = find_service_by_id(ctx, service_id);
    if (!service) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Verify memory belongs to service
    if (!polycall_micro_verify_memory_ownership(ctx, service_id, ptr)) {
        return POLYCALL_MICRO_ERROR_MEMORY;
    }
    
    // In a real implementation, we would free the memory in our allocator
    // For simplicity, we'll just zero the memory
    size_t offset = (uint8_t*)ptr - service->memory_pool;
    memset(ptr, 0, POLYCALL_MICRO_BUFFER_SIZE - offset);
    
    return POLYCALL_MICRO_SUCCESS;
}

/**
 * @brief Verify memory belongs to service
 */
bool polycall_micro_verify_memory_ownership(
    const PolycallMicroContext* ctx,
    uint32_t service_id,
    const void* ptr
) {
    if (!ctx || service_id == 0 || !ptr) {
        return false;
    }
    
    // Find service
    const PolycallServiceState* service = NULL;
    for (uint32_t i = 0; i < POLYCALL_MICRO_MAX_SERVICES; i++) {
        if ((ctx->service_array.active_mask & (1U << i)) && 
            ctx->service_array.services[i].id == service_id) {
            service = &ctx->service_array.services[i];
            break;
        }
    }
    
    if (!service) {
        return false;
    }
    
    // Check if pointer is within memory pool
    const uint8_t* byte_ptr = (const uint8_t*)ptr;
    const uint8_t* pool_start = service->memory_pool;
    const uint8_t* pool_end = pool_start + POLYCALL_MICRO_BUFFER_SIZE;
    
    return (byte_ptr >= pool_start && byte_ptr < pool_end);
}

/**
 * @brief Securely zero memory in service pool
 */
PolycallMicroStatus polycall_micro_secure_zero_memory(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    void* ptr,
    size_t size
) {
    if (!ctx || service_id == 0 || !ptr || size == 0) {
        return POLYCALL_MICRO_ERROR_MEMORY;
    }
    
    // Find service
    PolycallServiceState* service = find_service_by_id(ctx, service_id);
    if (!service) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Verify memory belongs to service
    if (!polycall_micro_verify_memory_ownership(ctx, service_id, ptr)) {
        return POLYCALL_MICRO_ERROR_MEMORY;
    }
    
    // Verify size doesn't exceed memory pool
    const uint8_t* byte_ptr = (const uint8_t*)ptr;
    const uint8_t* pool_start = service->memory_pool;
    size_t offset = byte_ptr - pool_start;
    
    if (offset + size > POLYCALL_MICRO_BUFFER_SIZE) {
        size = POLYCALL_MICRO_BUFFER_SIZE - offset;
    }
    
    // Securely zero memory
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    while (size--) {
        *p++ = 0;
    }
    
    return POLYCALL_MICRO_SUCCESS;
}

/**
 * @brief Collect garbage
 */
PolycallMicroStatus polycall_micro_collect_garbage(
    PolycallMicroContext* ctx
) {
    if (!ctx) {
        return POLYCALL_MICRO_ERROR_MEMORY;
    }
    
    uint64_t current_time = get_current_timestamp();
    uint64_t timeout = 3600; // 1 hour timeout for inactive services
    
    // Collect garbage from services (data-oriented batch processing)
    for (uint32_t i = 0; i < POLYCALL_MICRO_MAX_SERVICES; i++) {
        if (!(ctx->service_array.active_mask & (1U << i))) {
            continue; // Skip inactive services
        }
        
        PolycallServiceState* service = &ctx->service_array.services[i];
        
        // Check if service has been inactive for too long
        if (current_time - service->last_update > timeout) {
            // Clean up service
            polycall_micro_destroy_service(ctx, service->id);
        }
    }
    
    // Update garbage collection timestamp
    ctx->service_array.last_gc = current_time;
    
    // Update context checksum
    ctx->checksum = calculate_checksum(ctx, offsetof(PolycallMicroContext, checksum));
    
    return POLYCALL_MICRO_SUCCESS;
}

/**
 * @brief Get active services count
 */
uint32_t polycall_micro_get_active_services(
    const PolycallMicroContext* ctx
) {
    if (!ctx) {
        return 0;
    }
    
    return ctx->service_array.count;
}

/**
 * @brief Get service by ID
 */
const PolycallServiceState* polycall_micro_get_service(
    const PolycallMicroContext* ctx,
    uint32_t service_id
) {
    if (!ctx || service_id == 0) {
        return NULL;
    }
    
    // Find service using bitmap for efficient lookup
    for (uint32_t i = 0; i < POLYCALL_MICRO_MAX_SERVICES; i++) {
        if ((ctx->service_array.active_mask & (1U << i)) && 
            ctx->service_array.services[i].id == service_id) {
            return &ctx->service_array.services[i];
        }
    }
    
    return NULL;
}

/**
 * @brief Get status string
 */
const char* polycall_micro_status_string(
    PolycallMicroStatus status
) {
    switch (status) {
        case POLYCALL_MICRO_SUCCESS:
            return "Success";
        case POLYCALL_MICRO_ERROR_INIT:
            return "Initialization error";
        case POLYCALL_MICRO_ERROR_SERVICE:
            return "Service error";
        case POLYCALL_MICRO_ERROR_COMMAND:
            return "Command error";
        case POLYCALL_MICRO_ERROR_PROTOCOL:
            return "Protocol error";
        case POLYCALL_MICRO_ERROR_MEMORY:
            return "Memory error";
        case POLYCALL_MICRO_ERROR_ENCRYPTION:
            return "Encryption error";
        case POLYCALL_MICRO_ERROR_INTEGRITY:
            return "Integrity verification failed";
        case POLYCALL_MICRO_ERROR_PERMISSION:
            return "Permission denied";
        case POLYCALL_MICRO_ERROR_ISOLATION_BREACH:
            return "Isolation breach detected";
        case POLYCALL_MICRO_ERROR_ZERO_TRUST_FAILURE:
            return "Zero trust verification failed";
        default:
            return "Unknown error";
    }
}

/**
 * @brief Calculate checksum for data
 */
uint32_t polycall_micro_calculate_checksum(
    const void* data,
    size_t size
) {
    return calculate_checksum(data, size);
}

/**
 * @brief Get library version
 */
const char* polycall_micro_get_version(void) {
    return POLYCALL_MICRO_VERSION;
}

/**
 * @brief Check if service exists
 */
bool polycall_micro_service_exists(
    const PolycallMicroContext* ctx,
    uint32_t service_id
) {
    if (!ctx || service_id == 0) {
        return false;
    }
    
    // Find service using bitmap for efficient lookup
    for (uint32_t i = 0; i < POLYCALL_MICRO_MAX_SERVICES; i++) {
        if ((ctx->service_array.active_mask & (1U << i)) && 
            ctx->service_array.services[i].id == service_id) {
            return true;
        }
    }
    
    return false;
}


/**
 * @brief Reset service to initial state
 */
PolycallMicroStatus polycall_micro_reset_service(
    PolycallMicroContext* ctx,
    uint32_t service_id
) {
    if (!ctx || service_id == 0) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Find service
    PolycallServiceState* service = find_service_by_id(ctx, service_id);
    if (!service) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Save service ID and name
    uint32_t id = service->id;
    char name[32];
    memcpy(name, service->service_name, sizeof(name));
    
    // Reset service state
    reset_service_state(service);
    
    // Restore ID and name
    service->id = id;
    memcpy(service->service_name, name, sizeof(service->service_name));
    service->creation_time = get_current_timestamp();
    service->last_update = service->creation_time;
    
    // Generate new security keys
    polycall_micro_generate_keys(ctx, service_id);
    
    // Update service checksum
    update_service_checksum(service);
    
    return POLYCALL_MICRO_SUCCESS;
}

/* Create specialized bank service */
PolycallMicroStatus polycall_micro_create_bank_service(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    const char* name
) {
    // Create base service
    PolycallMicroStatus status = polycall_micro_create_service(ctx, service_id, 
                                                          POLYCALL_MICRO_FLAG_BANK_SERVICE | 
                                                          POLYCALL_MICRO_FLAG_ENCRYPTED |
                                                          POLYCALL_MICRO_FLAG_INTEGRITY |
                                                          POLYCALL_MICRO_FLAG_SECURE_MEMORY,
                                                          name);
    
    if (status != POLYCALL_MICRO_SUCCESS) {
        return status;
    }
    
    // Find service
    PolycallServiceState* service = find_service_by_id(ctx, service_id);
    if (!service) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Configure bank service with enhanced security
    service->security.key_rotation_interval = 3600; // 1 hour key rotation
    service->security.allowed_service_mask = 0; // No services allowed by default
    
    // Update security checksum
    service->security.checksum = calculate_checksum(&service->security, 
                                                  offsetof(PolycallSecurityContext, checksum));
    
    // Update service checksum
    update_service_checksum(service);
    
    return POLYCALL_MICRO_SUCCESS;
}

/**
 * @brief Create specialized advertisement service
 */
PolycallMicroStatus polycall_micro_create_ads_service(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    const char* name
) {
    // Create base service
    PolycallMicroStatus status = polycall_micro_create_service(ctx, service_id, 
                                                          POLYCALL_MICRO_FLAG_ADS_SERVICE |
                                                          POLYCALL_MICRO_FLAG_INTEGRITY,
                                                          name);
    
    if (status != POLYCALL_MICRO_SUCCESS) {
        return status;
    }
    
    // Find service
    PolycallServiceState* service = find_service_by_id(ctx, service_id);
    if (!service) {
        return POLYCALL_MICRO_ERROR_SERVICE;
    }
    
    // Configure ad service with privacy controls
    service->security.allowed_service_mask = 0; // No services allowed by default
    
    // Update security checksum
    service->security.checksum = calculate_checksum(&service->security, 
                                                  offsetof(PolycallSecurityContext, checksum));
    
    // Update service checksum
    update_service_checksum(service);
    
    return POLYCALL_MICRO_SUCCESS;
}

/**
 * @brief Verify system integrity
 */
PolycallMicroStatus polycall_micro_verify_system_integrity(
    PolycallMicroContext* ctx
) {
    if (!ctx) {
        return POLYCALL_MICRO_ERROR_INTEGRITY;
    }
    
    // Verify context checksum
    uint32_t context_checksum = calculate_checksum(
        ctx, 
        offsetof(PolycallMicroContext, checksum)
    );
    if (context_checksum != ctx->checksum) {
        return POLYCALL_MICRO_ERROR_INTEGRITY;
    }
    // Verify service array checksum
    uint32_t service_array_checksum = calculate_checksum(
        &ctx->service_array, 
        offsetof(PolycallServiceArray, checksum)
    );
    if (service_array_checksum != ctx->service_array.checksum) {
        return POLYCALL_MICRO_ERROR_INTEGRITY;
    }
    
    // Batch process integrity checks for all services
    for (uint32_t i = 0; i < POLYCALL_MICRO_MAX_SERVICES; i++) {
        if (!(ctx->service_array.active_mask & (1U << i))) {
            continue; // Skip inactive services
        }
        
        PolycallServiceState* service = &ctx->service_array.services[i];
        
        // Verify individual service integrity
        PolycallMicroStatus service_status = polycall_micro_verify_service_integrity(
            ctx, 
            service->id
        );
        
        if (service_status != POLYCALL_MICRO_SUCCESS) {
            return service_status;
        }
    }
    
    return POLYCALL_MICRO_SUCCESS;
}