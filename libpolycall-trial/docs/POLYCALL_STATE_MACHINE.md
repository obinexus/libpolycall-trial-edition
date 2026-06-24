# PolyCall State Machine Documentation

## Overview
The PolyCall State Machine is a robust, flexible implementation of a finite state machine (FSM) designed for high-reliability systems. It provides features such as state integrity checking, transition management, and state snapshots.

## Core Features
- State management with entry/exit actions
- Transition control with guard conditions
- State integrity verification
- State locking mechanism
- State snapshot and restoration
- Comprehensive diagnostics

## Usage

### Initialization
```c
polycall_context_t ctx;
PolyCall_StateMachine* sm;
initialize_state_machine(&ctx, &sm);
```

### State Configuration
The state machine comes with default states:
- INIT
- READY
- RUNNING
- PAUSED
- ERROR

### Default Transitions
Predefined transitions include:
- to_ready (INIT → READY)
- to_running (READY → RUNNING)
- to_paused (RUNNING → PAUSED)
- pause_to_running (PAUSED → RUNNING)
- to_error (RUNNING → ERROR)

## API Reference

### State Management

#### Adding States
```c
polycall_sm_status_t polycall_sm_add_state(
    PolyCall_StateMachine* sm,
    const char* name,
    PolyCall_StateAction on_enter,
    PolyCall_StateAction on_exit,
    bool is_final
);
```

#### State Locking
```c
polycall_sm_status_t polycall_sm_lock_state(
    PolyCall_StateMachine* sm,
    unsigned int state_id
);

polycall_sm_status_t polycall_sm_unlock_state(
    PolyCall_StateMachine* sm,
    unsigned int state_id
);
```

### Transition Management

#### Adding Transitions
```c
polycall_sm_status_t polycall_sm_add_transition(
    PolyCall_StateMachine* sm,
    const char* name,
    unsigned int from_state,
    unsigned int to_state,
    PolyCall_StateAction action,
    bool (*guard_condition)(const PolyCall_State*, const PolyCall_State*)
);
```

#### Executing Transitions
```c
polycall_sm_status_t polycall_sm_execute_transition(
    PolyCall_StateMachine* sm,
    const char* transition_name
);
```

### State Integrity

#### Verification
```c
polycall_sm_status_t polycall_sm_verify_state_integrity(
    PolyCall_StateMachine* sm,
    unsigned int state_id
);
```

### Snapshot Management

#### Creating Snapshots
```c
polycall_sm_status_t polycall_sm_create_state_snapshot(
    const PolyCall_StateMachine* sm,
    unsigned int state_id,
    PolyCall_StateSnapshot* snapshot
);
```

#### Restoring from Snapshots
```c
polycall_sm_status_t polycall_sm_restore_state_from_snapshot(
    PolyCall_StateMachine* sm,
    const PolyCall_StateSnapshot* snapshot
);
```

## Error Handling

The state machine uses return codes to indicate operation status:
- POLYCALL_SM_SUCCESS
- POLYCALL_SM_ERROR_INVALID_CONTEXT
- POLYCALL_SM_ERROR_NOT_INITIALIZED
- POLYCALL_SM_ERROR_INVALID_STATE
- POLYCALL_SM_ERROR_MAX_STATES_REACHED
- POLYCALL_SM_ERROR_INTEGRITY_CHECK_FAILED
- POLYCALL_SM_ERROR_STATE_LOCKED
- POLYCALL_SM_ERROR_VERSION_MISMATCH

## Best Practices

1. Always verify state integrity after critical operations
2. Use guard conditions to validate transitions
3. Implement proper error handling
4. Create state snapshots before critical transitions
5. Maintain state locks during sensitive operations

## Thread Safety
The state machine is not inherently thread-safe. Implement appropriate synchronization mechanisms when using in multi-threaded environments.

## License
This software is part of the PolyCall library. See LICENSE file for details.
