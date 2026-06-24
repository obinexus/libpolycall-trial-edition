#ifndef POLYCALL_UTILS_H
#define POLYCALL_UTILS_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @file function_utils.h
 * @brief Utilities for functional programming patterns in C
 *
 * This file provides utilities for implementing point-free style
 * and data-oriented programming in C, including function composition,
 * currying, and higher-order functions.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function pointer type for transformations without context
 * 
 * @param data Pointer to data to transform
 * @param size Size of the data in bytes
 * @return Transformed data or NULL on error
 */
typedef void* (*Transform)(const void* data, size_t size);

/**
 * @brief Function pointer type for transformations with context
 * 
 * @param data Pointer to data to transform
 * @param size Size of the data in bytes
 * @param ctx User-provided context
 * @return Transformed data or NULL on error
 */
typedef void* (*TransformWithContext)(const void* data, size_t size, void* ctx);

/**
 * @brief Function pointer type for predicates without context
 * 
 * @param data Pointer to data to evaluate
 * @param size Size of the data in bytes
 * @return true if predicate is satisfied, false otherwise
 */
typedef bool (*Predicate)(const void* data, size_t size);

/**
 * @brief Function pointer type for predicates with context
 * 
 * @param data Pointer to data to evaluate
 * @param size Size of the data in bytes
 * @param ctx User-provided context
 * @return true if predicate is satisfied, false otherwise
 */
typedef bool (*PredicateWithContext)(const void* data, size_t size, void* ctx);

/**
 * @brief Transformation chain structure
 * 
 * Represents a sequence of transformations to apply in order
 */
typedef struct {
    Transform* transforms;     /**< Array of transformation functions */
    size_t count;              /**< Number of transformations */
    void** contexts;           /**< Array of contexts (optional) */
    bool owns_contexts;        /**< Whether contexts should be freed */
} TransformChain;

/**
 * @brief Predicate chain structure
 * 
 * Represents a sequence of predicates to evaluate
 */
typedef struct {
    Predicate* predicates;     /**< Array of predicate functions */
    size_t count;              /**< Number of predicates */
    void** contexts;           /**< Array of contexts (optional) */
    bool owns_contexts;        /**< Whether contexts should be freed */
    bool any_match;            /**< If true, returns true if any predicate matches */
} PredicateChain;

/**
 * @brief Pipeline structure
 * 
 * Represents a processing pipeline combining transformations and predicates
 */
typedef struct {
    TransformChain transforms; /**< Transformation chain */
    PredicateChain predicates; /**< Predicate chain for filtering */
} Pipeline;

/**
 * @brief Create a transformation chain
 * 
 * @param transforms Array of transformation functions
 * @param count Number of transformations
 * @return TransformChain structure
 */
TransformChain polycall_create_transform_chain(Transform* transforms, size_t count);

/**
 * @brief Create a transformation chain with contexts
 * 
 * @param transforms Array of transformation functions with context
 * @param contexts Array of context pointers
 * @param count Number of transformations
 * @param owns_contexts Whether the chain should free contexts
 * @return TransformChain structure
 */
TransformChain polycall_create_transform_chain_with_context(
    TransformWithContext* transforms,
    void** contexts,
    size_t count,
    bool owns_contexts
);

/**
 * @brief Free resources associated with a transformation chain
 * 
 * @param chain TransformChain to destroy
 */
void polycall_destroy_transform_chain(TransformChain* chain);

/**
 * @brief Apply a transformation chain to data
 * 
 * @param chain TransformChain to apply
 * @param data Input data
 * @param size Size of input data
 * @param out_size Pointer to receive size of output data
 * @return Transformed data or NULL on error
 */
void* polycall_apply_transform_chain(
    const TransformChain* chain,
    const void* data,
    size_t size,
    size_t* out_size
);

/**
 * @brief Create a predicate chain
 * 
 * @param predicates Array of predicate functions
 * @param count Number of predicates
 * @param any_match If true, returns true if any predicate matches
 * @return PredicateChain structure
 */
PredicateChain polycall_create_predicate_chain(
    Predicate* predicates,
    size_t count,
    bool any_match
);

/**
 * @brief Create a predicate chain with contexts
 * 
 * @param predicates Array of predicate functions with context
 * @param contexts Array of context pointers
 * @param count Number of predicates
 * @param any_match If true, returns true if any predicate matches
 * @param owns_contexts Whether the chain should free contexts
 * @return PredicateChain structure
 */
PredicateChain polycall_create_predicate_chain_with_context(
    PredicateWithContext* predicates,
    void** contexts,
    size_t count,
    bool any_match,
    bool owns_contexts
);

/**
 * @brief Free resources associated with a predicate chain
 * 
 * @param chain PredicateChain to destroy
 */
void polycall_destroy_predicate_chain(PredicateChain* chain);

/**
 * @brief Evaluate a predicate chain against data
 * 
 * @param chain PredicateChain to evaluate
 * @param data Input data
 * @param size Size of input data
 * @return true if predicate chain is satisfied, false otherwise
 */
bool polycall_evaluate_predicate_chain(
    const PredicateChain* chain,
    const void* data,
    size_t size
);

/**
 * @brief Create a processing pipeline
 * 
 * @param transforms TransformChain for data transformation
 * @param predicates PredicateChain for data filtering
 * @return Pipeline structure
 */
Pipeline polycall_create_pipeline(
    TransformChain transforms,
    PredicateChain predicates
);

/**
 * @brief Free resources associated with a pipeline
 * 
 * @param pipeline Pipeline to destroy
 */
void polycall_destroy_pipeline(Pipeline* pipeline);

/**
 * @brief Apply a processing pipeline to data
 * 
 * @param pipeline Pipeline to apply
 * @param data Input data
 * @param size Size of input data
 * @param out_size Pointer to receive size of output data
 * @return Processed data or NULL on error/filtering
 */
void* polycall_apply_pipeline(
    const Pipeline* pipeline,
    const void* data,
    size_t size,
    size_t* out_size
);

/**
 * @brief Utility to compose two transformation functions
 * 
 * @param first First transformation to apply
 * @param second Second transformation to apply
 * @return Composed transformation function
 */
Transform polycall_compose_transforms(Transform first, Transform second);

/**
 * @brief Utility to compose two predicate functions (logical AND)
 * 
 * @param first First predicate to evaluate
 * @param second Second predicate to evaluate
 * @return Composed predicate function
 */
Predicate polycall_compose_predicates_and(Predicate first, Predicate second);

/**
 * @brief Utility to compose two predicate functions (logical OR)
 * 
 * @param first First predicate to evaluate
 * @param second Second predicate to evaluate
 * @return Composed predicate function
 */
Predicate polycall_compose_predicates_or(Predicate first, Predicate second);

#ifdef __cplusplus
}
#endif

#endif /* POLYCALL_UTILS_H */