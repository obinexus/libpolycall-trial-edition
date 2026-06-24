# PolyCall Protocol Documentation

## Overview
The PolyCall protocol implements a state-driven network communication protocol with support for handshaking, authentication, and reliable message delivery.

## Protocol Version
Current version: `POLYCALL_PROTOCOL_VERSION 1`

## Message Types
```c
enum {
    POLYCALL_MSG_HANDSHAKE = 0x01  // Initial connection handshake
    POLYCALL_MSG_AUTH = 0x02       // Authentication request/response
    POLYCALL_MSG_COMMAND = 0x03    // Command message
    POLYCALL_MSG_RESPONSE = 0x04   // Command response
    POLYCALL_MSG_ERROR = 0x05      // Error notification
    POLYCALL_MSG_HEARTBEAT = 0x06  // Connection keepalive
}
```

## Protocol States
```c
enum {
    POLYCALL_STATE_INIT      // Initial state
    POLYCALL_STATE_HANDSHAKE // Handshaking
    POLYCALL_STATE_AUTH      // Authentication
    POLYCALL_STATE_READY     // Ready for commands
    POLYCALL_STATE_ERROR     // Error state
    POLYCALL_STATE_CLOSED    // Connection closed
}
```

## Message Flags
- `POLYCALL_FLAG_ENCRYPTED` (0x01): Message payload is encrypted
- `POLYCALL_FLAG_COMPRESSED` (0x02): Message payload is compressed
- `POLYCALL_FLAG_URGENT` (0x04): High priority message
- `POLYCALL_FLAG_RELIABLE` (0x08): Guaranteed delivery required

## Message Structure

### Header Format
```c
struct {
    uint8_t version;          // Protocol version
    uint8_t type;            // Message type
    uint16_t flags;          // Message flags  
    uint32_t sequence;       // Sequence number
    uint32_t payload_length; // Payload size
    uint32_t checksum;       // Payload checksum
}
```

### Handshake Message
```c
struct {
    uint32_t magic;     // Protocol magic "PLC"
    uint8_t version;    // Protocol version
    uint16_t flags;     // Connection flags
}
```

## Protocol Flow

1. **Connection Establishment**
   - Client connects to server
   - State: `POLYCALL_STATE_INIT`

2. **Handshake**
   - Client sends `POLYCALL_MSG_HANDSHAKE`
   - Server validates version/magic
   - State: `POLYCALL_STATE_HANDSHAKE`

3. **Authentication** 
   - Client sends `POLYCALL_MSG_AUTH`
   - Server validates credentials
   - State: `POLYCALL_STATE_AUTH`

4. **Ready State**
   - Authentication successful
   - Command messages allowed
   - State: `POLYCALL_STATE_READY`

## Error Handling

- Protocol errors transition to `POLYCALL_STATE_ERROR`
- Error messages include error description string
- Only transition from ERROR is to CLOSED state

## Checksum Algorithm
```c
uint32_t checksum = 0;
for (byte in payload):
    checksum = ((checksum << 5) | (checksum >> 27)) + byte
```

## API Functions

### Initialization
```c
bool polycall_protocol_init(context, pc_ctx, endpoint, config)
void polycall_protocol_cleanup(context) 
```

### Message Handling
```c
bool polycall_protocol_send(context, type, payload, length, flags)
bool polycall_protocol_process(context, data, length)
```

### State Management
```c
void polycall_protocol_update(context)
polycall_protocol_state_t polycall_protocol_get_state(context)
bool polycall_protocol_can_transition(context, target_state)
```

### Connection Flow
```c
bool polycall_protocol_start_handshake(context)
bool polycall_protocol_complete_handshake(context)
bool polycall_protocol_authenticate(context, credentials, length)
```

### Status Checks
```c
bool polycall_protocol_is_connected(context)
bool polycall_protocol_is_authenticated(context)
bool polycall_protocol_is_error(context)
```

## Implementation Details

- Maximum message size: 4096 bytes
- Protocol timeout: 5000ms
- Thread-safe message handling
- Built-in state machine for transitions
- Support for user-defined callbacks
- Extensible message types
- Backward compatibility checks
