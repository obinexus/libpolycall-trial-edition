# ProtocolHandler

The ProtocolHandler class manages the PolyCall protocol implementation for reliable message handling and communication between clients and servers.

## Installation

```javascript
const { ProtocolHandler, MESSAGE_TYPES, PROTOCOL_FLAGS } = require('node-polycall');
```

## Protocol Constants

### Message Types
- `HANDSHAKE`: Initial connection handshake
- `AUTH`: Authentication messages
- `COMMAND`: Command execution requests 
- `RESPONSE`: Response messages
- `ERROR`: Error messages
- `HEARTBEAT`: Connection keepalive messages

### Protocol Flags
- `NONE`: No special handling
- `ENCRYPTED`: Message payload is encrypted
- `COMPRESSED`: Message payload is compressed
- `URGENT`: High priority message
- `RELIABLE`: Guaranteed delivery required

## Constructor

```javascript
const handler = new ProtocolHandler(options);
```

### Options
- `version`: Protocol version (default: 1)
- `checksumAlgorithm`: Algorithm for checksums (default: 'sha256')
- `encryption`: Enable encryption (default: false)
- `compression`: Enable compression (default: false)

## Core Methods

### Message Handling
```javascript
await handler.processMessage(data);
await handler.sendMessage(type, payload, flags);
handler.createMessage(type, payload, flags);
```

### Protocol Verification
```javascript
handler.verifyMessage(data);
handler.validateHeader(header);
handler.calculateChecksum(data);
```

### Protocol State
```javascript
handler.reset();
```

## Events

The ProtocolHandler emits the following events:

- `handshake`: When handshake completes
- `authenticated`: When authentication succeeds
- `command`: When command is received
- `response`: When response is received  
- `error`: When error occurs
- `heartbeat`: When heartbeat received
- `send`: When message is ready to send

## Implementation Details

### Message Structure
```
+----------------+------------------+
|    Header      |     Payload     |
| (16 bytes)     | (variable size) |
+----------------+------------------+
```

### Header Format
- Byte 0: Protocol version
- Byte 1: Message type
- Bytes 2-3: Flags
- Bytes 4-7: Sequence number
- Bytes 8-11: Payload length
- Bytes 12-15: Checksum

## Example Usage

```javascript
const handler = new ProtocolHandler({
    encryption: true,
    compression: true
});

// Handle incoming messages
handler.on('command', ({sequence, command, flags}) => {
    console.log('Received command:', command);
});

handler.on('error', (error) => {
    console.error('Protocol error:', error);
});

// Send a command
try {
    const result = await handler.sendMessage(
        MESSAGE_TYPES.COMMAND,
        'status',
        PROTOCOL_FLAGS.RELIABLE
    );
    console.log('Command sent successfully');
} catch (error) {
    console.error('Failed to send command:', error);
}
```

## Best Practices

1. Always verify message checksums
2. Handle protocol errors gracefully 
3. Implement proper message timeouts
4. Use encryption for sensitive data
5. Keep payload sizes reasonable
6. Maintain sequence number order
7. Process messages asynchronously

## See Also

- [PolyCallClient Documentation](./PolyCallClient.md)
- [NetworkEndpoint Documentation](./NetworkEndpoint.md)
- [StateMachine Documentation](./StateMachine.md)
