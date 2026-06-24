# Polycall Tokenizer Documentation

The Polycall tokenizer provides a robust lexical analysis system for tokenizing source code with a focus on performance, safety and composability.

## Overview

The tokenizer uses a pattern-matching and consumer-based approach to process input text into tokens. Key features include:

- Configurable buffer sizes and limits
- Composable token operations 
- Rich error reporting
- Position tracking
- Memory-safe implementation
- Support for numbers, strings, identifiers and operators

## Core Types

### PolycallTokenizerState

Bit flags representing tokenizer states:

```c
typedef enum {
    TOKENIZER_STATE_READY     = 0x01,
    TOKENIZER_STATE_SCANNING  = 0x02,
    TOKENIZER_STATE_ERROR     = 0x04,
    TOKENIZER_STATE_EOF       = 0x08,
    TOKENIZER_STATE_COMMENT   = 0x10,
    TOKENIZER_STATE_STRING    = 0x20,
    TOKENIZER_STATE_NUMBER    = 0x40,
    TOKENIZER_STATE_IDENTIFIER = 0x80
} PolycallTokenizerState;
```

### TokenPattern and TokenConsumer

Pattern matchers and token consumers enable composable tokenization:

```c
typedef struct {
    bool (*match)(const char* input, size_t* length);
    PolycallTokenType produces;
} TokenPattern;

typedef struct {
    void (*consume)(PolycallToken* token); 
    PolycallTokenType accepts;
} TokenConsumer;
```

## Core Functions

### Creation and Cleanup

```c
PolycallTokenizer* polycall_tokenizer_create(const PolycallTokenizerConfig* config);
void polycall_tokenizer_destroy(PolycallTokenizer* tokenizer);
void polycall_tokenizer_reset(PolycallTokenizer* tokenizer);
```

### Input Processing

```c
bool polycall_tokenizer_set_input(PolycallTokenizer* tokenizer, const char* input, size_t length);
bool polycall_tokenizer_process(PolycallTokenizer* tokenizer, const TokenizerOperations* ops);
```

### Pattern Matching

```c
bool polycall_tokenizer_match_identifier(const char* input, size_t* length);
bool polycall_tokenizer_match_number(const char* input, size_t* length);
bool polycall_tokenizer_match_string(const char* input, size_t* length);
bool polycall_tokenizer_match_operator(const char* input, size_t* length);
```

## Operation Composition

The tokenizer supports composable operations through:

```c
TokenizerOperations* polycall_tokenizer_create_ops(TokenPattern* patterns, TokenConsumer* consumers, size_t count);
TokenizerOperations* polycall_tokenizer_compose_ops(const TokenizerOperations* ops1, const TokenizerOperations* ops2);
void polycall_tokenizer_destroy_ops(TokenizerOperations* ops);
```

## Example Usage

```c
// Create tokenizer with default config
PolycallTokenizer* tokenizer = polycall_tokenizer_create(NULL);

// Define patterns and consumers
TokenPattern patterns[] = {
    { polycall_tokenizer_match_identifier, TOKEN_IDENTIFIER },
    { polycall_tokenizer_match_number, TOKEN_NUMBER }
};

TokenConsumer consumers[] = {
    { my_identifier_consumer, TOKEN_IDENTIFIER },
    { my_number_consumer, TOKEN_NUMBER }
};

// Create operations
TokenizerOperations* ops = polycall_tokenizer_create_ops(patterns, consumers, 2);

// Process input
const char* input = "x = 42";
polycall_tokenizer_set_input(tokenizer, input, strlen(input));
polycall_tokenizer_process(tokenizer, ops);

// Get results
const PolycallTokenArray* tokens = polycall_tokenizer_get_tokens(tokenizer);

// Cleanup
polycall_tokenizer_destroy_ops(ops);
polycall_tokenizer_destroy(tokenizer);
```

## Performance Considerations

- Token structures are optimized for cache alignment
- Operations can be composed for batch processing
- Pattern matching avoids excessive memory allocation
- Input buffering provides efficient streaming

## Error Handling

The tokenizer provides detailed error information:

```c
const char* error = polycall_tokenizer_get_error(tokenizer);
PolycallTokenizerState state = polycall_tokenizer_get_state(tokenizer);
```

## Thread Safety

The tokenizer is not thread-safe by default. Users must implement their own synchronization when sharing tokenizer instances across threads.