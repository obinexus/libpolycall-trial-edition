#ifndef POLYCALL_MICRO_H
#define POLYCALL_MICRO_H

#define POLYCALL_MICRO_VERSION "1.0.0"

#include <time.h>
#include <stdint.h>
#include <string.h>
#include "core/polycall.h"
#include "core/polycall_utils.h" 
#include "network/polycall_network.h"
#include "protocol/polycall_protocol.h"
#include "state/polycall_state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file polycall_micro.h
 * @brief Zero-trust microservice architecture for web component isolation
 *
 * This module implements a strict isolation framework for web components (such as
 * bank login, add services) ensuring complete data separation, memory isolation,
 * and secure communication channels between components using a data-oriented
 * programming (DOP) approach with point-free style operations.
 *
 * Security model assumes all components can be compromised and enforces boundaries
 * that prevent lateral movement between services.
 */

/******************************************************************************
 * Constants and Configuration
 ******************************************************************************/

#define POLYCALL_MEMORY_GUARD_MAGIC 0xDEADBEEF
/** Maximum number of isolated microservices */
#define POLYCALL_MICRO_MAX_SERVICES 32

/** Maximum number of network endpoints per service */
#define POLYCALL_MICRO_MAX_ENDPOINTS 8

/** Maximum number of commands in processing queue */
#define POLYCALL_MICRO_MAX_COMMANDS 64

/** Buffer size for memory operations */
#define POLYCALL_MICRO_BUFFER_SIZE 4096

/** Encryption key length in bytes */
#define POLYCALL_MICRO_KEY_LENGTH 32

/** Service isolation flags */
typedef enum {
    POLYCALL_MICRO_FLAG_NONE           = 0x00000000,
    POLYCALL_MICRO_FLAG_ENCRYPTED      = 0x00000001,  /**< Enable encryption */
    POLYCALL_MICRO_FLAG_ISOLATED       = 0x00000002,  /**< Strict isolation */
    POLYCALL_MICRO_FLAG_INTEGRITY      = 0x00000004,  /**< Integrity verification */
    POLYCALL_MICRO_FLAG_AUDIT          = 0x00000008,  /**< Audit logging */
    POLYCALL_MICRO_FLAG_SECURE_MEMORY  = 0x00000010,  /**< Secure memory operations */
    POLYCALL_MICRO_FLAG_ZERO_TRUST     = 0x00000020,  /**< Zero trust verification */
    POLYCALL_MICRO_FLAG_BANK_SERVICE   = 0x00010000,  /**< Bank service flags */
    POLYCALL_MICRO_FLAG_ADS_SERVICE    = 0x00020000   /**< Ad service flags */
} PolycallMicroFlags;

/** Status codes for operations */
typedef enum {
    POLYCALL_MICRO_SUCCESS = 0,              /**< Operation successful */
    POLYCALL_MICRO_ERROR_INIT,               /**< Initialization error */
    POLYCALL_MICRO_ERROR_SERVICE,            /**< Service error */
    POLYCALL_MICRO_ERROR_COMMAND,            /**< Command error */
    POLYCALL_MICRO_ERROR_PROTOCOL,           /**< Protocol error */
    POLYCALL_MICRO_ERROR_MEMORY,             /**< Memory error */
    POLYCALL_MICRO_ERROR_ENCRYPTION,         /**< Encryption error */
    POLYCALL_MICRO_ERROR_INTEGRITY,          /**< Integrity verification failed */
    POLYCALL_MICRO_ERROR_PERMISSION,         /**< Permission denied */
    POLYCALL_MICRO_ERROR_ISOLATION_BREACH,   /**< Isolation breach detected */
    POLYCALL_MICRO_ERROR_ZERO_TRUST_FAILURE  /**< Zero trust verification failed */
} PolycallMicroStatus;

/******************************************************************************
 * Data Structures
 ******************************************************************************/

/**
 * @brief Secure command structure with integrity verification
 *
 * Data-oriented design with contiguous memory layout for cache efficiency
 * and minimal pointer indirection. Commands are cryptographically signed
 * for zero-trust verification.
 */
typedef struct {
    uint32_t id;                            /**< Command ID */
    uint32_t flags;                         /**< Command flags */
    uint32_t service_id;                    /**< Originating service ID */
    uint32_t target_service_id;             /**< Target service ID (0 for broadcast) */
    uint32_t payload_size;                  /**< Payload size */
    uint64_t timestamp;                     /**< Creation timestamp */
    uint64_t expiration;                    /**< Expiration timestamp */
    uint8_t payload[POLYCALL_MICRO_BUFFER_SIZE]; /**< Command payload */
    uint8_t signature[POLYCALL_MICRO_KEY_LENGTH]; /**< Command signature */
    uint32_t checksum;                      /**< Command checksum */
} PolycallCommand;

/**
 * @brief Command array for batch processing
 *
 * Contiguous memory array for efficient batch processing of commands
 * using a data-oriented approach.
 */
typedef struct {
    PolycallCommand commands[POLYCALL_MICRO_MAX_COMMANDS]; /**< Command array */
    uint32_t count;                         /**< Command count */
    uint32_t capacity;                      /**< Array capacity */
    uint32_t checksum;                      /**< Array checksum */
} PolycallCommandArray;

/**
 * @brief Service memory guard for isolation enforcement
 *
 * Guards memory boundaries between isolated services
 */
typedef struct {
    uint32_t magic;                         /**< Magic identifier */
    uint32_t service_id;                    /**< Service ID */
    uint32_t size;                          /**< Size of protected memory */
    uint32_t checksum;                      /**< Memory checksum */
} PolycallMemoryGuard;

/**
 * @brief Service security context
 *
 * Contains security parameters for service isolation
 */
typedef struct {
    uint8_t encryption_key[POLYCALL_MICRO_KEY_LENGTH]; /**< Service encryption key */
    uint8_t hmac_key[POLYCALL_MICRO_KEY_LENGTH];      /**< Service HMAC key */
    uint64_t key_generation_time;                     /**< Key generation timestamp */
    uint64_t key_rotation_interval;                   /**< Key rotation interval */
    uint32_t access_control_mask;                     /**< Access control bitmask */
    uint32_t allowed_service_mask;                    /**< Allowed services bitmask */
    uint32_t checksum;                                /**< Security context checksum */
} PolycallSecurityContext;

/**
 * @brief Isolated service state
 *
 * Complete isolated state for a single microservice with memory protection,
 * network isolation, and security controls. Uses data-oriented design for
 * cache-friendly memory layout and minimal pointer indirection.
 */
typedef struct {
    /* Service identification */
    uint32_t id;                            /**< Service ID */
    uint32_t flags;                         /**< Service flags */
    uint32_t state;                         /**< Service state */
    uint64_t creation_time;                 /**< Creation timestamp */
    uint64_t last_update;                   /**< Last update timestamp */
    char service_name[32];                  /**< Service name */
    
    /* Memory isolation */
    PolycallMemoryGuard guard_pre;          /**< Memory guard (prefix) */
    uint8_t memory_pool[POLYCALL_MICRO_BUFFER_SIZE]; /**< Private memory pool */
    PolycallMemoryGuard guard_post;         /**< Memory guard (postfix) */
    
    /* Network isolation */
    polycall_network_endpoint_t endpoints[POLYCALL_MICRO_MAX_ENDPOINTS]; /**< Network endpoints */
    uint32_t endpoint_count;                /**< Number of endpoints */
    uint32_t endpoint_mask;                 /**< Active endpoint mask */
    
    /* Command processing */
    PolycallCommandArray command_queue;     /**< Command queue */
    
    /* Security context */
    PolycallSecurityContext security;       /**< Security context */
    
    /* Integrity verification */
    uint32_t checksum;                      /**< State checksum */
} PolycallServiceState;

/**
 * @brief Service array for contiguous state storage
 *
 * Stores all isolated services in contiguous memory for efficient access
 * and minimal cache misses.
 */
typedef struct {
    PolycallServiceState services[POLYCALL_MICRO_MAX_SERVICES]; /**< Services array */
    uint32_t count;                         /**< Service count */
    uint32_t active_mask;                   /**< Active service mask */
    uint64_t last_gc;                       /**< Last garbage collection */
    uint32_t checksum;                      /**< Array checksum */
} PolycallServiceArray;

/**
 * @brief Microservice context
 *
 * Main context for the microservice architecture with data-oriented design.
 */
typedef struct {
    PolycallServiceArray service_array;     /**< Service array */
    PolyCall_StateMachine* state_machine;   /**< State machine */
    polycall_protocol_context_t protocol_ctx; /**< Protocol context */
    uint32_t flags;                         /**< Context flags */
    uint64_t startup_time;                  /**< Startup timestamp */
    uint32_t checksum;                      /**< Context checksum */
} PolycallMicroContext;

/******************************************************************************
 * Function Type Definitions for Point-Free Style
 ******************************************************************************/

/**
 * @brief Command transformation function
 *
 * Pure function pointer type for point-free command transformations,
 * allowing composable command processing pipelines.
 *
 * @param cmd Command to transform
 */
typedef void (*PolycallTransform)(PolycallCommand* cmd);

/**
 * @brief Command predicate function
 *
 * Pure function pointer type for point-free command filtering,
 * enabling composable command filtering pipelines.
 *
 * @param cmd Command to evaluate
 * @return true if predicate is satisfied, false otherwise
 */
typedef bool (*PolycallPredicate)(const PolycallCommand* cmd);

/**
 * @brief Service operation function
 *
 * Pure function pointer type for point-free service operations,
 * allowing composable service state transformation pipelines.
 *
 * @param service Service to operate on
 */
typedef void (*PolycallOperation)(PolycallServiceState* service);

/**
 * @brief Service predicate function
 *
 * Pure function pointer type for point-free service filtering,
 * enabling composable service filtering pipelines.
 *
 * @param service Service to evaluate
 * @return true if predicate is satisfied, false otherwise
 */
typedef bool (*PolycallServicePredicate)(const PolycallServiceState* service);

/**
 * @brief Command transformation chain
 *
 * Chain of transformations to apply in sequence to commands,
 * following point-free style for function composition.
 */
typedef struct {
    PolycallTransform* transforms;          /**< Transformation array */
    uint32_t transform_count;               /**< Transformation count */
    uint32_t checksum;                      /**< Chain checksum */
} PolycallTransformChain;

/**
 * @brief Command predicate chain
 *
 * Chain of predicates to apply in sequence to commands,
 * following point-free style for function composition.
 */
typedef struct {
    PolycallPredicate* predicates;          /**< Predicate array */
    uint32_t predicate_count;               /**< Predicate count */
    bool require_all;                       /**< Require all predicates (AND) */
    uint32_t checksum;                      /**< Chain checksum */
} PolycallPredicateChain;

/******************************************************************************
 * Core API Functions
 ******************************************************************************/

/**
 * @brief Initialize microservice context
 *
 * @param ctx Context to initialize
 * @param config Configuration parameters
 * @return Status code
 */
PolycallMicroStatus polycall_micro_init(
    PolycallMicroContext* ctx,
    const polycall_config_t* config
);

/**
 * @brief Clean up microservice context
 *
 * @param ctx Context to clean up
 */
void polycall_micro_cleanup(PolycallMicroContext* ctx);

/**
 * @brief Create isolated microservice
 *
 * Creates a new isolated service with its own memory pool, network endpoints,
 * and security context. Service isolation ensures that data from one service
 * cannot be accessed by another, even if one service is compromised.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param flags Service flags
 * @param name Service name
 * @return Status code
 */
PolycallMicroStatus polycall_micro_create_service(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    uint32_t flags,
    const char* name
);

/**
 * @brief Destroy isolated microservice
 *
 * @param ctx Context
 * @param service_id Service ID
 * @return Status code
 */
PolycallMicroStatus polycall_micro_destroy_service(
    PolycallMicroContext* ctx,
    uint32_t service_id
);

/**
 * @brief Verify service isolation integrity
 *
 * Performs comprehensive integrity checks to ensure service isolation
 * boundaries have not been compromised. Uses cryptographic verification
 * to detect tampering with memory, state, or communication channels.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @return Status code
 */
PolycallMicroStatus polycall_micro_verify_service_integrity(
    PolycallMicroContext* ctx,
    uint32_t service_id
);

/**
 * @brief Seal service against modification
 *
 * Prevents further modifications to service configuration
 * and enables strict isolation verification.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @return Status code
 */
PolycallMicroStatus polycall_micro_seal_service(
    PolycallMicroContext* ctx,
    uint32_t service_id
);

/**
 * @brief Update service state
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param new_state New state
 * @return Status code
 */
PolycallMicroStatus polycall_micro_update_service_state(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    uint32_t new_state
);

/******************************************************************************
 * Network Isolation Functions
 ******************************************************************************/

/**
 * @brief Create isolated network endpoint for service
 *
 * Creates a new network endpoint with dedicated port and
 * isolation controls to prevent cross-service network access.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param port Port number
 * @param protocol Network protocol
 * @param role Network role
 * @return Status code
 */
PolycallMicroStatus polycall_micro_create_endpoint(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    uint16_t port,
    polycall_network_protocol_t protocol,
    polycall_network_role_t role
);

/**
 * @brief Close service network endpoint
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param endpoint_index Endpoint index
 * @return Status code
 */
PolycallMicroStatus polycall_micro_close_endpoint(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    uint32_t endpoint_index
);

/**
 * @brief Configure service network isolation
 *
 * Sets up network isolation parameters to prevent unauthorized
 * cross-service communication. Implements network segmentation
 * using service-specific encryption and authentication.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param allowed_services Mask of allowed services
 * @param allowed_ports Array of allowed ports
 * @param port_count Number of ports
 * @return Status code
 */
PolycallMicroStatus polycall_micro_configure_network_isolation(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    uint32_t allowed_services,
    const uint16_t* allowed_ports,
    uint32_t port_count
);

/******************************************************************************
 * Command Processing Functions (Point-Free Style)
 ******************************************************************************/

/**
 * @brief Create secure command
 *
 * Creates a new command with integrity protection and authentication.
 * Commands are cryptographically signed to ensure they can only be
 * executed by authorized services.
 *
 * @param ctx Context
 * @param service_id Source service ID
 * @param target_service_id Target service ID
 * @param payload Command payload
 * @param payload_size Payload size
 * @param cmd Output command
 * @return Status code
 */
PolycallMicroStatus polycall_micro_create_command(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    uint32_t target_service_id,
    const void* payload,
    uint32_t payload_size,
    PolycallCommand* cmd
);

/**
 * @brief Apply transformation to command
 *
 * Applies a transformation function to modify a command.
 * Uses point-free style for composable command processing.
 *
 * @param cmd Command to transform
 * @param chain Transformation chain
 * @return Status code
 */
PolycallMicroStatus polycall_micro_transform_command(
    PolycallCommand* cmd,
    const PolycallTransformChain* chain
);

/**
 * @brief Filter commands using predicate
 *
 * Uses a predicate function to filter commands.
 * Implements point-free style for composable filtering.
 *
 * @param commands Command array
 * @param predicate Predicate function
 * @return Status code
 */
PolycallMicroStatus polycall_micro_filter_commands(
    PolycallCommandArray* commands,
    PolycallPredicate predicate
);

/**
 * @brief Process service using operation
 *
 * Applies an operation function to modify service state.
 * Uses point-free style for composable service operations.
 *
 * @param service Service to process
 * @param operation Operation function
 * @return Status code
 */
PolycallMicroStatus polycall_micro_process_service(
    PolycallServiceState* service,
    PolycallOperation operation
);

/**
 * @brief Batch process commands for service
 *
 * Efficiently processes multiple commands for a service.
 * Implements data-oriented batch processing for performance.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param commands Command array
 * @return Status code
 */
PolycallMicroStatus polycall_micro_batch_process(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    const PolycallCommandArray* commands
);

/**
 * @brief Execute command securely
 *
 * Executes a command with full security verification.
 * Command is validated against zero-trust security model
 * before execution in isolated service context.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param cmd Command to execute
 * @return Status code
 */
PolycallMicroStatus polycall_micro_execute_command(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    const PolycallCommand* cmd
);

/******************************************************************************
 * Function Composition Utilities (Point-Free Style)
 ******************************************************************************/

/**
 * @brief Create transformation chain
 *
 * Creates a chain of transforms to apply in sequence.
 * Enables point-free function composition for command processing.
 *
 * @param transforms Array of transforms
 * @param count Number of transforms
 * @return Transformation chain
 */
PolycallTransformChain polycall_micro_create_transform_chain(
    PolycallTransform* transforms,
    uint32_t count
);

/**
 * @brief Destroy transformation chain
 *
 * @param chain Transformation chain
 */
void polycall_micro_destroy_transform_chain(
    PolycallTransformChain* chain
);

/**
 * @brief Compose predicate functions (AND)
 *
 * Combines two predicates with logical AND.
 * Implements point-free function composition.
 *
 * @param first First predicate
 * @param second Second predicate
 * @return Composed predicate
 */
PolycallPredicate polycall_micro_compose_predicates_and(
    PolycallPredicate first,
    PolycallPredicate second
);

/**
 * @brief Compose predicate functions (OR)
 *
 * Combines two predicates with logical OR.
 * Implements point-free function composition.
 *
 * @param first First predicate
 * @param second Second predicate
 * @return Composed predicate
 */
PolycallPredicate polycall_micro_compose_predicates_or(
    PolycallPredicate first,
    PolycallPredicate second
);

/**
 * @brief Compose operation functions
 *
 * Combines two operations in sequence.
 * Implements point-free function composition.
 *
 * @param first First operation
 * @param second Second operation
 * @return Composed operation
 */
PolycallOperation polycall_micro_compose_operations(
    PolycallOperation first,
    PolycallOperation second
);

/**
 * @brief Map operation over services
 *
 * Applies an operation to multiple services.
 * Implements point-free higher-order function.
 *
 * @param ctx Context
 * @param operation Operation to apply
 * @param predicate Optional service selection predicate
 * @return Status code
 */
PolycallMicroStatus polycall_micro_map_operation(
    PolycallMicroContext* ctx,
    PolycallOperation operation,
    PolycallServicePredicate predicate
);

/******************************************************************************
 * Security Functions
 ******************************************************************************/

/**
 * @brief Generate security keys for service
 *
 * Generates cryptographic keys for service isolation.
 * Uses secure random number generation for key material.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @return Status code
 */
PolycallMicroStatus polycall_micro_generate_keys(
    PolycallMicroContext* ctx,
    uint32_t service_id
);

/**
 * @brief Rotate security keys for service
 *
 * Periodically rotates cryptographic keys for forward secrecy.
 * Ensures compromise of current keys doesn't affect future communications.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @return Status code
 */
PolycallMicroStatus polycall_micro_rotate_keys(
    PolycallMicroContext* ctx,
    uint32_t service_id
);

/**
 * @brief Set access control for service
 *
 * Configures access control permissions for a service.
 * Implements principle of least privilege for service operations.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param access_mask Access control bitmask
 * @return Status code
 */
PolycallMicroStatus polycall_micro_set_access_control(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    uint32_t access_mask
);

/**
 * @brief Verify command signature
 *
 * Cryptographically verifies command authenticity.
 * Implements zero-trust verification for commands.
 *
 * @param ctx Context
 * @param cmd Command to verify
 * @return Status code
 */
PolycallMicroStatus polycall_micro_verify_command(
    PolycallMicroContext* ctx,
    const PolycallCommand* cmd
);

/**
 * @brief Sign command with service key
 *
 * Cryptographically signs command to ensure authenticity.
 * Enables zero-trust verification by recipients.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param cmd Command to sign
 * @return Status code
 */
PolycallMicroStatus polycall_micro_sign_command(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    PolycallCommand* cmd
);

/******************************************************************************
 * Memory Isolation Functions
 ******************************************************************************/

/**
 * @brief Allocate memory from service pool
 *
 * Allocates memory from service's isolated memory pool.
 * Ensures memory operations stay within service boundaries.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param size Allocation size
 * @return Allocated memory or NULL
 */
void* polycall_micro_service_alloc(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    size_t size
);

/**
 * @brief Free memory from service pool
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param ptr Memory pointer
 * @return Status code
 */
PolycallMicroStatus polycall_micro_service_free(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    void* ptr
);

/**
 * @brief Verify memory belongs to service
 *
 * Checks if a memory pointer is within service's memory pool.
 * Prevents cross-service memory access attempts.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param ptr Memory pointer
 * @return true if memory belongs to service
 */
bool polycall_micro_verify_memory_ownership(
    const PolycallMicroContext* ctx,
    uint32_t service_id,
    const void* ptr
);

/**
 * @brief Securely zero memory in service pool
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param ptr Memory pointer
 * @param size Memory size
 * @return Status code
 */
PolycallMicroStatus polycall_micro_secure_zero_memory(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    void* ptr,
    size_t size
);

/******************************************************************************
 * Resource Management Functions
 ******************************************************************************/

/**
 * @brief Collect garbage
 *
 * Reclaims resources from inactive services.
 * Implements resource lifecycle management for services.
 *
 * @param ctx Context
 * @return Status code
 */
PolycallMicroStatus polycall_micro_collect_garbage(
    PolycallMicroContext* ctx
);

/**
 * @brief Get active services count
 *
 * @param ctx Context
 * @return Number of active services
 */
uint32_t polycall_micro_get_active_services(
    const PolycallMicroContext* ctx
);

/**
 * @brief Get service by ID
 *
 * @param ctx Context
 * @param service_id Service ID
 * @return Service state or NULL
 */
const PolycallServiceState* polycall_micro_get_service(
    const PolycallMicroContext* ctx,
    uint32_t service_id
);

/******************************************************************************
 * Utility Functions
 ******************************************************************************/

/**
 * @brief Get status string
 *
 * @param status Status code
 * @return Status description
 */
const char* polycall_micro_status_string(
    PolycallMicroStatus status
);

/**
 * @brief Calculate checksum for data
 *
 * Computes cryptographic checksum for data integrity verification.
 * Used for memory integrity and tamper detection.
 *
 * @param data Data pointer
 * @param size Data size
 * @return Checksum
 */
uint32_t polycall_micro_calculate_checksum(
    const void* data,
    size_t size
);

/**
 * @brief Get library version
 *
 * @return Version string
 */
const char* polycall_micro_get_version(void);

/**
 * @brief Find service by ID
 *
 * Internal helper function to locate a service by its ID.
 * 
 * @param ctx Context
 * @param service_id Service ID
 * @return Service state pointer or NULL if not found
 */
PolycallServiceState* find_service_by_id(
    const PolycallMicroContext* ctx,
    uint32_t service_id
);

/**
 * @brief Reset service state to initial values
 *
 * Internal helper function to reset a service's state.
 *
 * @param service Service to reset
 */
void reset_service_state(PolycallServiceState* service);


/**
 * @brief Check if service exists
 *
 * @param ctx Context
 * @param service_id Service ID
 * @return true if service exists
 */
bool polycall_micro_service_exists(
    const PolycallMicroContext* ctx,
    uint32_t service_id
);

/**
 * @brief Reset service to initial state
 *
 * @param ctx Context
 * @param service_id Service ID
 * @return Status code
 */
PolycallMicroStatus polycall_micro_reset_service(
    PolycallMicroContext* ctx,
    uint32_t service_id
);

/**
 * @brief Create specialized bank service
 *
 * Creates a bank service with enhanced security controls.
 * Implements financial-grade security for banking operations.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param name Service name
 * @return Status code
 */
PolycallMicroStatus polycall_micro_create_bank_service(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    const char* name
);

/**
 * @brief Create specialized advertisement service
 *
 * Creates an advertisement service with appropriate isolation.
 * Implements privacy controls for advertisement operations.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @param name Service name
 * @return Status code
 */
PolycallMicroStatus polycall_micro_create_ads_service(
    PolycallMicroContext* ctx,
    uint32_t service_id,
    const char* name
);

/******************************************************************************
 * Experimental Integrity Verification Functions
 ******************************************************************************/

/**
 * @brief Verify system integrity
 *
 * Performs comprehensive system-wide integrity checks.
 * Validates all isolation boundaries and security controls.
 *
 * @param ctx Context
 * @return Status code
 */
PolycallMicroStatus polycall_micro_verify_system_integrity(
    PolycallMicroContext* ctx
);

/**
 * @brief Test isolation boundary
 *
 * Attempts to access across service boundaries.
 * Should always fail if isolation is working correctly.
 *
 * @param ctx Context
 * @param source_id Source service ID
 * @param target_id Target service ID
 * @return Status code (should be POLYCALL_MICRO_ERROR_ISOLATION_BREACH)
 */
PolycallMicroStatus polycall_micro_test_isolation_boundary(
    PolycallMicroContext* ctx,
    uint32_t source_id,
    uint32_t target_id
);

/**
 * @brief Create debug service
 *
 * Creates a special service for debug operations.
 * Only available in debug builds, not in production.
 *
 * @param ctx Context
 * @param service_id Service ID
 * @return Status code
 */
#ifdef POLYCALL_DEBUG
PolycallMicroStatus polycall_micro_create_debug_service(
    PolycallMicroContext* ctx,
    uint32_t service_id
);
#endif

#ifdef __cplusplus
}
#endif

#endif /* POLYCALL_MICRO_H */