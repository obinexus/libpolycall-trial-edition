# PolyCall Micro Library Documentation

## Overview

The PolyCall Micro library provides a lightweight microservices framework built on top of the core PolyCall library. It implements a data-oriented design focused on efficient state management and command processing.

## Key Features

- Data-oriented microservice architecture 
- Efficient batch command processing
- Service lifecycle management
- Point-free style transformations
- Automatic garbage collection
- Protocol and state machine integration

## Core Components

### Command Structure
```c
typedef struct {
    uint32_t id;              // Command identifier
    uint32_t flags;           // Command flags
    uint32_t payload_size;    // Size of payload data
    uint8_t payload[];        // Variable-length payload
} PolycallCommand;
```

### Service State
```c
typedef struct {
    uint32_t id;              // Service identifier
    uint32_t flags;           // Service flags
    uint32_t state;           // Current service state
    uint64_t last_update;     // Last update timestamp
    NetworkEndpoint endpoints[]; // Service endpoints
} PolycallServiceState;
```

## API Reference

### Initialization
```c
PolycallMicroStatus polycall_micro_init(
    PolycallMicroContext* ctx,
    const polycall_config_t* config
);
```

### Service Management
```c
PolycallMicroStatus polycall_micro_create_service();
PolycallMicroStatus polycall_micro_destroy_service();
PolycallMicroStatus polycall_micro_update_service_state();
```

### Command Processing
```c
PolycallMicroStatus polycall_micro_transform_command();
PolycallMicroStatus polycall_micro_filter_commands();
PolycallMicroStatus polycall_micro_batch_process();
```

### Resource Management
```c
PolycallMicroStatus polycall_micro_collect_garbage();
void polycall_micro_cleanup();
```

## Error Handling

Status codes:
- POLYCALL_MICRO_SUCCESS
- POLYCALL_MICRO_ERROR_INIT
- POLYCALL_MICRO_ERROR_SERVICE
- POLYCALL_MICRO_ERROR_COMMAND
- POLYCALL_MICRO_ERROR_PROTOCOL
- POLYCALL_MICRO_ERROR_MEMORY