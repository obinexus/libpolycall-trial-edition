#include "core/polycall_utils.h"
#include <stdlib.h>
#include <string.h>

/* Transform chain implementation */

TransformChain polycall_create_transform_chain(Transform* transforms, size_t count) {
    TransformChain chain = {
        .transforms = NULL,
        .count = 0,
        .contexts = NULL,
        .owns_contexts = false
    };
    
    if (!transforms || count == 0) {
        return chain;
    }
    
    chain.transforms = malloc(count * sizeof(Transform));
    if (!chain.transforms) {
        return chain;
    }
    
    memcpy(chain.transforms, transforms, count * sizeof(Transform));
    chain.count = count;
    
    return chain;
}

/* Wrapper for context-based transforms */
typedef struct {
    TransformWithContext func;
    void* context;
} TransformWrapper;

static void* transform_context_wrapper(const void* data __attribute__((unused)),
                                     size_t size __attribute__((unused))) {
    /* This is never actually called directly as we handle the context separately */
    return NULL;
}

TransformChain polycall_create_transform_chain_with_context(
    TransformWithContext* transforms,
    void** contexts,
    size_t count,
    bool owns_contexts
) {
    TransformChain chain = {
        .transforms = NULL,
        .count = 0,
        .contexts = NULL,
        .owns_contexts = owns_contexts
    };
    
    if (!transforms || !contexts || count == 0) {
        return chain;
    }
    
    /* For each transform with context, we create a wrapper struct that 
     * holds both the function and its context */
    TransformWrapper** wrappers = malloc(count * sizeof(TransformWrapper*));
    if (!wrappers) {
        return chain;
    }
    
    /* Create an array of transform functions that all use the same signature */
    chain.transforms = malloc(count * sizeof(Transform));
    chain.contexts = malloc(count * sizeof(void*));
    
    if (!chain.transforms || !chain.contexts) {
        free(wrappers);
        free(chain.transforms);
        free(chain.contexts);
        chain.transforms = NULL;
        chain.contexts = NULL;
        chain.count = 0;
        return chain;
    }
    
    /* Set up each transform and its context */
    for (size_t i = 0; i < count; i++) {
        wrappers[i] = malloc(sizeof(TransformWrapper));
        if (!wrappers[i]) {
            /* Cleanup on error */
            for (size_t j = 0; j < i; j++) {
                free(wrappers[j]);
            }
            free(wrappers);
            free(chain.transforms);
            free(chain.contexts);
            chain.transforms = NULL;
            chain.contexts = NULL;
            chain.count = 0;
            return chain;
        }
        
        wrappers[i]->func = transforms[i];
        wrappers[i]->context = contexts[i];
        
        /* Each function in the chain is the same wrapper function,
         * the actual transform happens during application */
        chain.transforms[i] = transform_context_wrapper;
        chain.contexts[i] = wrappers[i];
    }
    
    chain.count = count;
    chain.owns_contexts = true; /* We always own the wrapper contexts */
    
    free(wrappers); /* Free the wrappers array but not the individual wrappers */
    return chain;
}

void polycall_destroy_transform_chain(TransformChain* chain) {
    if (!chain) return;
    
    if (chain->transforms) {
        free(chain->transforms);
        chain->transforms = NULL;
    }
    
    if (chain->contexts) {
        if (chain->owns_contexts) {
            for (size_t i = 0; i < chain->count; i++) {
                /* If this is a wrapped transform, free the wrapper */
                TransformWrapper* wrapper = (TransformWrapper*)chain->contexts[i];
                free(wrapper);
            }
        }
        free(chain->contexts);
        chain->contexts = NULL;
    }
    
    chain->count = 0;
}

void* polycall_apply_transform_chain(
    const TransformChain* chain,
    const void* data,
    size_t size,
    size_t* out_size
) {
    if (!chain || !data || size == 0 || !out_size) {
        return NULL;
    }
    
    /* No transformations to apply */
    if (chain->count == 0 || !chain->transforms) {
        void* result = malloc(size);
        if (!result) return NULL;
        
        memcpy(result, data, size);
        *out_size = size;
        return result;
    }
    
    /* Apply first transformation */
    size_t current_size = size;
    void* current_data = NULL;
    
    /* Handle the first transform */
    if (chain->contexts && chain->contexts[0]) {
        /* Check if this is a wrapper context */
        TransformWrapper* wrapper = (TransformWrapper*)chain->contexts[0];
        current_data = wrapper->func(data, size, wrapper->context);
    } else {
        /* Regular transform without context */
        Transform transform = chain->transforms[0];
        current_data = transform(data, size);
    }
    
    if (!current_data) {
        return NULL;
    }
    
    /* Apply remaining transformations */
    for (size_t i = 1; i < chain->count; i++) {
        void* next_data = NULL;
        
        if (chain->contexts && chain->contexts[i]) {
            /* Check if this is a wrapper context */
            TransformWrapper* wrapper = (TransformWrapper*)chain->contexts[i];
            next_data = wrapper->func(current_data, current_size, wrapper->context);
        } else {
            /* Regular transform without context */
            Transform transform = chain->transforms[i];
            next_data = transform(current_data, current_size);
        }
        
        free(current_data);
        
        if (!next_data) {
            return NULL;
        }
        
        current_data = next_data;
    }
    
    *out_size = current_size;
    return current_data;
}

/* Predicate chain implementation follows similar pattern */
typedef struct {
    PredicateWithContext func;
    void* context;
} PredicateWrapper;
static bool predicate_context_wrapper(const void* data __attribute__((unused)),
                                    size_t size __attribute__((unused))) {
    /* This is never actually called directly as we handle the context separately */
    return false;
}

PredicateChain polycall_create_predicate_chain(
    Predicate* predicates,
    size_t count,
    bool any_match
) {
    PredicateChain chain = {
        .predicates = NULL,
        .count = 0,
        .contexts = NULL,
        .owns_contexts = false,
        .any_match = any_match
    };
    
    if (!predicates || count == 0) {
        return chain;
    }
    
    chain.predicates = malloc(count * sizeof(Predicate));
    if (!chain.predicates) {
        return chain;
    }
    
    memcpy(chain.predicates, predicates, count * sizeof(Predicate));
    chain.count = count;
    
    return chain;
}

PredicateChain polycall_create_predicate_chain_with_context(
    PredicateWithContext* predicates,
    void** contexts,
    size_t count,
    bool any_match,
    bool owns_contexts __attribute__((unused))
) {
    PredicateChain chain = {
        .predicates = NULL,
        .count = 0,
        .contexts = NULL,
        .owns_contexts = false,
        .any_match = any_match
    };
    
    if (!predicates || !contexts || count == 0) {
        return chain;
    }
    
    /* For each predicate with context, create a wrapper */
    PredicateWrapper** wrappers = malloc(count * sizeof(PredicateWrapper*));
    if (!wrappers) {
        return chain;
    }
    
    chain.predicates = malloc(count * sizeof(Predicate));
    chain.contexts = malloc(count * sizeof(void*));
    
    if (!chain.predicates || !chain.contexts) {
        free(wrappers);
        free(chain.predicates);
        free(chain.contexts);
        chain.predicates = NULL;
        chain.contexts = NULL;
        chain.count = 0;
        return chain;
    }
    
    /* Set up each predicate and its context */
    for (size_t i = 0; i < count; i++) {
        wrappers[i] = malloc(sizeof(PredicateWrapper));
        if (!wrappers[i]) {
            /* Cleanup on error */
            for (size_t j = 0; j < i; j++) {
                free(wrappers[j]);
            }
            free(wrappers);
            free(chain.predicates);
            free(chain.contexts);
            chain.predicates = NULL;
            chain.contexts = NULL;
            chain.count = 0;
            return chain;
        }
        
        wrappers[i]->func = predicates[i];
        wrappers[i]->context = contexts[i];
        
        /* Each function in the chain is the same wrapper */
        chain.predicates[i] = predicate_context_wrapper;
        chain.contexts[i] = wrappers[i];
    }
    
    chain.count = count;
    chain.owns_contexts = true; /* We always own the wrapper contexts */
    
    free(wrappers);
    return chain;
}

void polycall_destroy_predicate_chain(PredicateChain* chain) {
    if (!chain) return;
    
    if (chain->predicates) {
        free(chain->predicates);
        chain->predicates = NULL;
    }
    
    if (chain->contexts) {
        if (chain->owns_contexts) {
            for (size_t i = 0; i < chain->count; i++) {
                /* If this is a wrapped predicate, free the wrapper */
                PredicateWrapper* wrapper = (PredicateWrapper*)chain->contexts[i];
                free(wrapper);
            }
        }
        free(chain->contexts);
        chain->contexts = NULL;
    }
    
    chain->count = 0;
}

bool polycall_evaluate_predicate_chain(
    const PredicateChain* chain,
    const void* data,
    size_t size
) {
    if (!chain || !data || size == 0) {
        return false;
    }
    
    /* No predicates to evaluate means everything passes */
    if (chain->count == 0 || !chain->predicates) {
        return true;
    }
    
    /* Track if we've found any matches for OR logic */
    bool found_match = false;
    
    for (size_t i = 0; i < chain->count; i++) {
        bool result;
        
        if (chain->contexts && chain->contexts[i]) {
            PredicateWrapper* wrapper = (PredicateWrapper*)chain->contexts[i];
            result = wrapper->func(data, size, wrapper->context);
        } else {
            result = chain->predicates[i](data, size);
        }
        
        if (chain->any_match) {
            if (result) {
                return true;  /* OR: Short-circuit on first true */
            }
        } else {
            if (!result) {
                return false; /* AND: Short-circuit on first false */
            }
        }
        
        found_match |= result;
    }
    
    /* OR: Return true if any predicate matched
     * AND: Return true if we got here (all predicates matched) */
    return chain->any_match ? found_match : true;
}

/* Pipeline implementation */

Pipeline polycall_create_pipeline(
    TransformChain transforms,
    PredicateChain predicates
) {
    Pipeline pipeline = {
        .transforms = transforms,
        .predicates = predicates
    };
    
    return pipeline;
}

void polycall_destroy_pipeline(Pipeline* pipeline) {
    if (!pipeline) return;
    
    polycall_destroy_transform_chain(&pipeline->transforms);
    polycall_destroy_predicate_chain(&pipeline->predicates);
}

Pipeline polycall_create_empty_pipeline(void) {
    Pipeline pipeline = {
        .transforms = {
            .transforms = NULL,
            .count = 0,
            .contexts = NULL,
            .owns_contexts = false
        },
        .predicates = {
            .predicates = NULL,
            .count = 0,
            .contexts = NULL,
            .owns_contexts = false,
            .any_match = false
        }
    };
    return pipeline;
}

void* polycall_apply_pipeline(
    const Pipeline* pipeline,
    const void* data,
    size_t size,
    size_t* out_size
) {
    if (!pipeline || !data || size == 0 || !out_size) {
        return NULL;
    }
    
    /* Apply predicates first (filtering) */
    bool passes_filter = polycall_evaluate_predicate_chain(&pipeline->predicates, data, size);
    if (!passes_filter) {
        return NULL;
    }
    
    /* Apply transformations */
    return polycall_apply_transform_chain(&pipeline->transforms, data, size, out_size);
}

/* Function composition utilities */

/* Wrapper for composed transforms */
typedef struct {
    Transform first;
    Transform second;
} ComposedTransformData;

static void* composed_transform(const void* data, size_t size, void* ctx) {
    ComposedTransformData* compose_data = (ComposedTransformData*)ctx;
    
    size_t intermediate_size = size;
    void* intermediate = compose_data->first(data, size);
    if (!intermediate) return NULL;
    
    void* result = compose_data->second(intermediate, intermediate_size);
    free(intermediate);
    
    return result;
}

/* Functions to create and apply composed transforms */
typedef struct {
    TransformWithContext func;
    void* context;
    bool owns_context;
} TransformWithLifetime;

static void* transform_applier(const void* data __attribute__((unused)),
                             size_t size __attribute__((unused))) {
    /* Never called directly */
    return NULL;
}

Transform polycall_compose_transforms(Transform first, Transform second) {
    if (!first || !second) return NULL;
    
    /* Allocate the composition data */
    ComposedTransformData* ctx = malloc(sizeof(ComposedTransformData));
    if (!ctx) return NULL;
    
    ctx->first = first;
    ctx->second = second;
    
    /* Create a wrapper that holds both the function and context */
    TransformWithLifetime* wrapper = malloc(sizeof(TransformWithLifetime));
    if (!wrapper) {
        free(ctx);
        return NULL;
    }
    
    wrapper->func = composed_transform;
    wrapper->context = ctx;
    wrapper->owns_context = true;
    
    /* Store the wrapper in a global registry and return a handle */
    /* In a real implementation, you would manage these wrappers */
    /* Here we leak memory for simplicity */
    
    return transform_applier;
}

/* Wrapper for composed predicates */
typedef struct {
    Predicate first;
    Predicate second;
    bool is_or;
} ComposedPredicateData;

static bool composed_predicate(const void* data, size_t size, void* ctx) {
    ComposedPredicateData* compose_data = (ComposedPredicateData*)ctx;
    
    bool first_result = compose_data->first(data, size);
    
    if (compose_data->is_or) {
        if (first_result) return true;  /* Short-circuit OR */
    } else {
        if (!first_result) return false;  /* Short-circuit AND */
    }
    
    return compose_data->second(data, size);
}

typedef struct {
    PredicateWithContext func;
    void* context;
    bool owns_context;
} PredicateWithLifetime;

static bool predicate_applier(const void* data __attribute__((unused)), 
                            size_t size __attribute__((unused))) {
    /* Never called directly */
    return false;
}

static PredicateWithLifetime* predicate_registry = NULL;
static size_t predicate_registry_size = 0;

static Predicate create_composed_predicate(Predicate first, Predicate second, bool is_or) {
    if (!first || !second) return NULL;
    
    /* Allocate the composition data */
    ComposedPredicateData* ctx = malloc(sizeof(ComposedPredicateData));
    if (!ctx) return NULL;
    
    ctx->first = first;
    ctx->second = second;
    ctx->is_or = is_or;
    
    /* Create a wrapper that holds both the function and context */
    PredicateWithLifetime* new_registry = realloc(predicate_registry, 
        (predicate_registry_size + 1) * sizeof(PredicateWithLifetime));
    if (!new_registry) {
        free(ctx);
        return NULL;
    }
    
    predicate_registry = new_registry;
    predicate_registry[predicate_registry_size].func = composed_predicate;
    predicate_registry[predicate_registry_size].context = ctx;
    predicate_registry[predicate_registry_size].owns_context = true;
    predicate_registry_size++;
    
    return predicate_applier;
}

Predicate polycall_compose_predicates_and(Predicate first, Predicate second) {
    return create_composed_predicate(first, second, false);
}

Predicate polycall_compose_predicates_or(Predicate first, Predicate second) {
    return create_composed_predicate(first, second, true);
}