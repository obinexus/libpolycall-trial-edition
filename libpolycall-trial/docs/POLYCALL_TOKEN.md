# Polycall Token Documentation

## Overview
The Polycall token system provides a robust tokenization infrastructure for parsing and lexical analysis. This module handles token creation, management, and manipulation with a focus on type safety and efficient memory usage.

## Core Types

### PolycallValueType
Enumerates the supported value types:
```c
typedef enum {
    VALUE_NONE = 0,
    VALUE_INTEGER,
    VALUE_FLOAT, 
    VALUE_STRING,
    VALUE_IDENTIFIER
} PolycallValueType;
```

### PolycallValue
A tagged union for type-safe value storage:
```c
typedef struct {
    PolycallValueType type;
    union {
        int64_t int_value;
        double float_value;
        struct {
            const char* data;
            uint32_t length;
        } string_value;
    } data;
} PolycallValue;
```

### PolycallTokenType
Bit flags for efficient token type comparison:
```c
typedef enum {
    TOKEN_INVALID     = 0x00,
    TOKEN_IDENTIFIER  = 0x01,
    TOKEN_NUMBER      = 0x02,
    TOKEN_STRING      = 0x04,
    TOKEN_OPERATOR    = 0x08,
    TOKEN_KEYWORD     = 0x10,
    TOKEN_SEPARATOR   = 0x20,
    TOKEN_COMMENT     = 0x40,
    TOKEN_EOF         = 0x80
} PolycallTokenType;
```

## Token Structure
The `PolycallToken` struct is optimized for cache alignment:
- Value (24 bytes)
- Type (4 bytes) 
- Flags (4 bytes)
- Line number (4 bytes)
- Column number (4 bytes)
- Length (4 bytes)

Total size: 44 bytes aligned

## Core Functions

### Array Management
```c
PolycallTokenArray* polycall_token_create_array(uint32_t capacity);
void polycall_token_destroy_array(PolycallTokenArray* array);
```

### Token Operations
```c
PolycallToken polycall_token_map(const PolycallToken* token, TokenOperation op);
PolycallTokenArray* polycall_token_filter(const PolycallTokenArray* array, TokenPredicate pred);
PolycallTokenArray* polycall_token_chain(const PolycallTokenArray* array, const PolycallTokenOperations* ops);
```

### Value Operations
```c
PolycallValue polycall_value_create(PolycallValueType type, const void* data);
void polycall_value_destroy(PolycallValue* value);
bool polycall_value_equals(const PolycallValue* a, const PolycallValue* b);
```

## Memory Management
The token system follows RAII principles:
- Arrays must be created with `polycall_token_create_array()`
- Arrays must be destroyed with `polycall_token_destroy_array()`
- Values must be properly initialized and destroyed

## Thread Safety
- Token operations are not thread-safe by default
- Synchronization must be handled by the caller
- Arrays should not be modified while being processed

## Performance Considerations
- Token structure is optimized for cache alignment
- Bit flags enable efficient type checking
- Batch operations are preferred over individual token processing

## Example Usage
```c
// Create token array
PolycallTokenArray* tokens = polycall_token_create_array(1024);

// Process tokens
PolycallTokenArray* filtered = polycall_token_filter(tokens, my_predicate);
PolycallToken mapped = polycall_token_map(&token, my_transform);

// Cleanup
polycall_token_destroy_array(filtered);
polycall_token_destroy_array(tokens);
```