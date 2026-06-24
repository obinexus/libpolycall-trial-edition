# NetworkEndpoint

The NetworkEndpoint class provides a flexible networking interface for both client and server operations in the PolyCall library. It extends EventEmitter to provide event-based communication.

## Installation

The NetworkEndpoint class is included in the PolyCall library:

```js
const { NetworkEndpoint } = require('node-polycall');
```

## Constructor

```js
new NetworkEndpoint(options = {})
```

### Options

- `host` (string): Server host (default: 'localhost')
- `port` (number): Server port (default: 8080)
- `backlog` (number): Connection backlog size (default: 1024)
- `timeout` (number): Socket timeout in ms (default: 5000)
- `keepAlive` (boolean): Enable keep-alive (default: true)
- `reconnect` (boolean): Auto-reconnect on disconnect (default: true)
- `maxRetries` (number): Max reconnection attempts (default: 3)
- `retryDelay` (number): Delay between retries in ms (default: 1000)

## Methods

### Server Operations

```js
async listen()          // Start server listening
async close()           // Close server
```

### Client Operations

```js
async connect()         // Connect to server
async disconnect()      // Disconnect from server
async send(data)       // Send data to server
```

### Utility Methods

```js
getAddress()           // Get current address info
isConnected()          // Check connection status
getProtocol()          // Get protocol handler
toString()             // Get string representation
```

## Events

- `listening`: Server started listening
- `connection`: New client connected
- `connected`: Client connected to server
- `disconnected`: Client/Server disconnected
- `reconnecting`: Client attempting reconnection
- `error`: Error occurred
- `socket:closed`: Socket closed
- `server:closed`: Server closed

## Example Usage

### Server Mode
```js
const endpoint = new NetworkEndpoint({ port: 8080 });

endpoint.on('listening', ({address}) => {
    console.log(`Server listening on ${address.port}`);
});

endpoint.on('connection', ({socket}) => {
    console.log('New client connected');
});

await endpoint.listen();
```

### Client Mode
```js
const endpoint = new NetworkEndpoint({
    host: 'localhost',
    port: 8080
});

endpoint.on('connected', () => {
    console.log('Connected to server');
});

endpoint.on('disconnected', () => {
    console.log('Disconnected from server');
});

await endpoint.connect();
await endpoint.send(data);
await endpoint.disconnect();
```

## Error Handling

```js
endpoint.on('error', (error) => {
    console.error('Network error:', error);
});

try {
    await endpoint.connect();
} catch (error) {
    console.error('Connection failed:', error);
}
```

## Implementation Details

- Uses Node.js net module for TCP connections
- Implements automatic reconnection with exponential backoff
- Handles pending data during reconnection
- Integrates with ProtocolHandler for message processing
- Thread-safe event handling

## See Also

- [PolyCallClient Documentation](./PolyCallClient.md)
- [ProtocolHandler Documentation](./ProtocolHandler.md)
- [Router Documentation](./Router.md)