# State

The State class provides a robust implementation for managing state within the PolyCall system. It extends EventEmitter to provide event-based communication.

## Overview

A State represents a discrete condition in the state machine with associated metadata, handlers, transitions and endpoints. Each state can be locked/unlocked and maintains versioning information.

## Constructor

```javascript
new State(name, options = {})
```

- `name` (string): Required unique name for the state
- `options` (object):
    - `endpoint` (string): Custom endpoint path (default: normalized name)
    - `timeout` (number): Handler timeout in ms (default: 5000)
    - `retryCount` (number): Handler retry attempts (default: 3)

## Properties

- `name`: State identifier
- `isLocked`: Boolean lock status
- `endpoint`: HTTP endpoint path
- `handlers`: Map of event handlers
- `metadata`: Map of state metadata
- `transitions`: Set of allowed transitions

## Methods

### Handler Management 

```javascript
addHandler(event, handler)
removeHandler(event)
executeHandler(event, ...args)
```

### State Control

```javascript
lock()
unlock()
```

### Transition Management

```javascript
addTransition(toState)
canTransitionTo(targetState)
```

### Endpoint Management

```javascript
getEndpoint()
setEndpoint(endpoint)
```

### Metadata Management

```javascript
setMetadata(key, value)
getMetadata(key)
```

### Serialization & Snapshots

```javascript
toJSON()
createSnapshot()
restoreFromSnapshot(snapshot)
```

## Events

- `locked`: Emitted when state is locked
- `unlocked`: Emitted when state is unlocked  
- `error`: Emitted on handler errors
- `restored`: Emitted after snapshot restoration

## Example Usage

```javascript
const state = new State('ready');

// Add handlers
state.addHandler('enter', async () => {
    console.log('Entering ready state');
});

// Add transitions
state.addTransition('processing');

// Set metadata
state.setMetadata('priority', 1);

// Lock state
state.lock();

// Create snapshot
const snapshot = state.createSnapshot();
```

## Error Handling

All methods that can fail will throw errors with descriptive messages. Common error scenarios:

- Invalid state name
- State already locked/unlocked
- Invalid handler type
- Missing handlers
- Invalid endpoint format
- Invalid snapshot format
- Checksum verification failure

## Implementation Notes

- Checksums are used to verify snapshot integrity
- Handlers are executed asynchronously 
- State locks prevent handler execution
- Metadata is versioned automatically
- Default endpoints are normalized state names