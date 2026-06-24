# Network Module Documentation

## Overview
The Network module provides a cross-platform networking interface that supports both TCP and UDP protocols for client-server communication. It includes thread-safe operations and non-blocking I/O.

## Key Components

### Network Types
```c
typedef enum {
    NET_TCP,            // TCP protocol
    NET_UDP,            // UDP protocol  
    NET_RAW            // Raw sockets
} NetworkProtocol;

typedef enum {
    NET_CLIENT,        // Client role
    NET_SERVER,        // Server role
    NET_PEER          // Peer-to-peer role
} NetworkRole;
```

### Core Structures

#### NetworkEndpoint
Represents a network communication endpoint:
```c
typedef struct {
    pthread_mutex_t lock;           // Thread safety mutex
    char address[INET_ADDRSTRLEN];  // IP address
    uint16_t port;                  // Port number
    NetworkProtocol protocol;       // Protocol type
    NetworkRole role;               // Endpoint role
    int socket_fd;                  // Socket descriptor
    struct sockaddr_in addr;        // Socket address
    void* user_data;               // Custom user data
} NetworkEndpoint;
```

#### NetworkProgram 
Manages network operations and client connections:
```c
typedef struct {
    NetworkEndpoint* endpoints;      // Endpoint array
    size_t count;                   // Number of endpoints
    ClientState clients[10];        // Client connections
    pthread_mutex_t clients_lock;    // Thread safety for clients
    volatile bool running;           // Program state
    struct {
        void (*on_receive)(NetworkEndpoint*, NetworkPacket*);
        void (*on_connect)(NetworkEndpoint*);
        void (*on_disconnect)(NetworkEndpoint*);
    } handlers;
} NetworkProgram;
```

## Core Functions

### Initialization and Cleanup
```c
bool net_init(NetworkEndpoint* endpoint);
void net_close(NetworkEndpoint* endpoint);
void net_init_program(NetworkProgram* program);
void net_cleanup_program(NetworkProgram* program);
```

### Network Operations
```c
ssize_t net_send(NetworkEndpoint* endpoint, NetworkPacket* packet);
ssize_t net_receive(NetworkEndpoint* endpoint, NetworkPacket* packet);
void net_run(NetworkProgram* program);
```

### Client Management
```c
void net_init_client_state(ClientState* state);
void net_cleanup_client_state(ClientState* state);
bool net_add_client(NetworkProgram* program, int socket_fd, struct sockaddr_in addr);
void net_remove_client(NetworkProgram* program, int socket_fd);
```

## Examples

### Initialize Server Program
```c
NetworkProgram* program = calloc(1, sizeof(NetworkProgram));
net_init_program(program);
program->handlers.on_receive = handle_data;
program->handlers.on_connect = handle_connect;
program->handlers.on_disconnect = handle_disconnect;
```

### Send Data
```c
NetworkPacket packet = {
    .data = buffer,
    .size = size,
    .flags = 0
};
net_send(endpoint, &packet);
```

## Platform Support
- Windows: Uses Winsock2 API
- Unix/Linux: Uses BSD sockets API
- Thread-safe operations using pthread mutexes
- Non-blocking I/O support

## Error Handling
```c
typedef enum {
    NET_SUCCESS = 0,
    NET_ERROR_SOCKET = -1,
    NET_ERROR_BIND = -2,
    NET_ERROR_LISTEN = -3,
    NET_ERROR_ACCEPT = -4,
    NET_ERROR_SEND = -5,
    NET_ERROR_RECEIVE = -6,
    NET_ERROR_MEMORY = -7,
    NET_ERROR_INVALID = -8
} NetworkError;
```

## Constants
```c
#define NET_MAX_CLIENTS 10
#define NET_BUFFER_SIZE 1024
#define NET_MAX_BACKLOG 5
#define NET_TIMEOUT_SEC 1
#define NET_TIMEOUT_USEC 0
```