# StateMachine

The StateMachine class manages state transitions and event handling in the PolyCall system. It orchestrates multiple State instances and enforces transition rules.

## Overview
StateMachine provides a framework for creating and managing complex state workflows with event handling, validation, and persistence capabilities.
Key features:

- **State Management**: Create and manage multiple named states
- **Transition Control**: Define allowed transitions between states with guard conditions 
- **Event Handling**: Emit events for state changes and transitions
- **History Tracking**: Record state transition history with timestamps
- **Validation**: Verify state configurations and transitions
- **Persistence**: Create and restore state machine snapshots
- **Error Handling**: Graceful error handling with detailed messaging

## Implementation

The StateMachine class extends EventEmitter to provide event-based notifications. It maintains internal Maps for states and transitions, along with configuration options and history tracking.

```javascript
const sm = new StateMachine({
    allowSelfTransitions: false,
    validateStateChange: true, 
    recordHistory: true,
    maxHistoryLength: 100
});
```
