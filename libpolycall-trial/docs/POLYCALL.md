# PolyCall Library Documentation

## Overview

PolyCall is a lightweight C library that provides a flexible state management system. This library is designed to be easy to use, efficient, and thread-safe, making it suitable for embedded systems and general-purpose applications.

## Version

Current Version: 1.0.0

## Features

- Configurable memory management
- Error handling with detailed error messages
- Thread-safe operations
- Support for up to 32 states and 64 transitions
- Minimal memory footprint
- C and C++ compatibility

## API Reference

### Types and Constants

#### Constants
```c
#define POLYCALL_MAX_NAME_LENGTH 32
#define POLYCALL_MAX_STATES 32
#define POLYCALL_MAX_TRANSITIONS 64
```

#### Status Codes
```c
typedef enum {
    POLYCALL_SUCCESS = 0,
    POLYCALL_ERROR_INVALID_PARAMETERS,
    POLYCALL_ERROR_INITIALIZATION_FAILED,
    POLYCALL_ERROR_OUT_OF_MEMORY,
    POLYCALL_ERROR
} polycall_status_t;
```

#### Configuration Structure
```c
typedef struct polycall_config {
    unsigned int flags;
    size_t memory_pool_size;
    void* user_data;
} polycall_config_t;
```

### Core Functions

#### Initialization
```c
polycall_status_t polycall_init_with_config(
    polycall_context_t* ctx, 
    const polycall_config_t* config
);
```
Initializes a new PolyCall context with the specified configuration.

#### Cleanup
```c
void polycall_cleanup(polycall_context_t ctx);
```
Releases resources associated with a PolyCall context.

#### Version Information
```c
const char* polycall_get_version(void);
```
Returns the current version of the PolyCall library.

#### Error Handling
```c
const char* polycall_get_last_error(polycall_context_t ctx);
```
Retrieves the last error message from the context.

## Usage Example

```c
#include "polycall.h"

int main() {
    polycall_context_t ctx;
    polycall_config_t config = {
        .flags = 0,
        .memory_pool_size = 1024 * 1024,  // 1MB
        .user_data = NULL
    };

    // Initialize context
    polycall_status_t status = polycall_init_with_config(&ctx, &config);
    if (status != POLYCALL_SUCCESS) {
        printf("Error: %s\n", polycall_get_last_error(ctx));
        return 1;
    }

    // Use the context...

    // Cleanup
    polycall_cleanup(ctx);
    return 0;
}
```

## Error Handling

The library provides comprehensive error handling through status codes and error messages. Always check the return status of functions that return `polycall_status_t`.

## Thread Safety

The library is designed to be thread-safe when used with separate contexts. Each context maintains its own error state and memory pool.

## Building and Linking

To use PolyCall in your project, include the header file and link against the library:

```bash
gcc -c your_program.c -I/path/to/polycall/include
gcc your_program.o -lpolycall -o your_program
```

## License

[Insert appropriate license information here]

## Contributing

[Insert contribution guidelines here]