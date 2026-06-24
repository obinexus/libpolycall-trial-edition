# PolyCallClient

The `PolyCallClient` class provides a high-level interface for interacting with the PolyCall server. It manages connections, authentication, state transitions, and command execution.

## Installation

```javascript
const { PolyCallClient } = require('node-polycall');
```

## Constructor

```javascript
const client = new PolyCallClient(options);
```

### Options
- `host` (string): Server host address (default: 'localhost')
- `port` (number): Server port (default: 8080) 
- `reconnect` (boolean): Enable auto-reconnection (default: true)
- `timeout` (number): Request timeout in ms (default: 5000)
- `maxRetries` (number): Max reconnection attempts (default: 3)

## Core Methods

### Connection Management
```javascript
await client.connect(); // Connect to server
await client.disconnect(); // Disconnect from server
client.isConnected(); // Check connection status
```

### Authentication
```javascript
await client.authenticate(credentials);
client.isAuthenticated(); // Check authentication status
```

### Command Execution
```javascript 
await client.executeCommand(command, data);
await client.sendRequest(path, method, data);
```

### State Management
```javascript
await client.transitionTo(stateName);
await client.getState(stateName);
await client.getAllStates();
await client.lockState(stateName);
await client.unlockState(stateName);
client.getCurrentState();
client.getStateHistory();
```

## Events

The client emits the following events:

- `connected`: When connection is established
- `disconnected`: When connection is lost 
- `authenticated`: When authentication succeeds
- `handshake`: When handshake completes
- `command`: When command is received
- `state:changed`: When state transition occurs
- `error`: When an error occurs

## Example Usage

```javascript
const client = new PolyCallClient({
    host: 'localhost',
    port: 8080
});

client.on('connected', () => {
    console.log('Connected to server');
});

client.on('authenticated', () => {
    console.log('Authenticated successfully'); 
});

try {
    await client.connect();
    await client.authenticate({
        username: 'test',
        password: 'test' 
    });
    
    const state = await client.getAllStates();
    console.log('Current states:', state);

    await client.transitionTo('ready');
    
    const result = await client.executeCommand('status');
    console.log('Command result:', result);

} catch (error) {
    console.error('Error:', error);
}
```

## Protocol Details

The client implements the PolyCall protocol with:

- Reliable message delivery
- Encryption support
- State management
- Heartbeat monitoring
- Auto-reconnection
- Checksum verification

## Error Handling

Errors are emitted through the `error` event and can be caught using try/catch:

```javascript
client.on('error', error => {
    console.error('Client error:', error);
});

try {
    await client.executeCommand('invalid');
} catch (error) {
    console.error('Command failed:', error);
}
```

## Debug Helpers

```javascript
client.printRoutes(); // Print registered routes
console.log(client.toString()); // Get string representation
```

## Implementation Details

The client uses:

- EventEmitter for event handling
- NetworkEndpoint for socket management
- Router for request routing
- StateMachine for state management
- ProtocolHandler for message processing

## See Also

- [Router Documentation](Router.md)
- [StateMachine Documentation](StateMachine.md)
- [NetworkEndpoint Documentation](NetworkEndpoint.md)
- [ProtocolHandler Documentation](ProtocolHandler.md)