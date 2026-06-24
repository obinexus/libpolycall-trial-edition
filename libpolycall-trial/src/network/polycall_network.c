/**
 * @file polycall_network.c
 * @brief Data-oriented network implementation for LibPolyCall
 *
 * This file implements the network layer with a focus on data-oriented design,
 * cache-friendly memory layouts, and point-free style operations.
 */

#include "polycall_network.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #define SHUT_RDWR SD_BOTH
    typedef char* sock_opt_type;
#else
    #define _POSIX_C_SOURCE 199309L
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <time.h>
    typedef void* sock_opt_type;
#endif

/* Constants for network operations */
#define NETWORK_MAGIC_NUMBER 0x504E4554 /* "PNET" */
#define NETWORK_DEFAULT_BACKLOG 16
#define NETWORK_DEFAULT_TIMEOUT_MS 1000
#define NETWORK_RETRY_COUNT 3
#define NETWORK_RETRY_DELAY_MS 500
#define NETWORK_MAX_BUFFER_SIZE 8192

/* Dynamic port range */
#define NETWORK_MIN_DYNAMIC_PORT 49152
#define NETWORK_MAX_DYNAMIC_PORT 65535

/* Error messages */
static const char* ERROR_SOCKET_CREATION = "Socket creation failed";
static const char* ERROR_SOCKET_OPTION = "Failed to set socket option";
static const char* ERROR_ADDRESS_BINDING = "Address binding failed";
static const char* ERROR_PORT_IN_USE = "Port already in use";
static const char* ERROR_CONNECTION_FAILED = "Connection failed";
static const char* ERROR_LISTEN_FAILED = "Listen operation failed";
static const char* ERROR_ACCEPT_FAILED = "Accept operation failed";
static const char* ERROR_SEND_FAILED = "Send operation failed";
static const char* ERROR_RECEIVE_FAILED = "Receive operation failed";
static const char* ERROR_NOT_INITIALIZED = "Network context not initialized";
static const char* ERROR_INVALID_PARAMETER = "Invalid parameter";
static const char* ERROR_WOULD_BLOCK = "Operation would block";
static const char* ERROR_TIMEOUT = "Operation timed out";

/**
 * Forward declarations of internal functions
 */
static NetSocket* find_available_socket(polycall_network_context_t* context);
static int set_socket_nonblocking(int sockfd);
static void close_socket(int sockfd);
static bool init_winsock(void);
static void cleanup_winsock(void);
static int get_last_socket_error(void);
static const char* socket_error_to_string(int error_code);
static uint64_t get_current_time_ms(void);
static bool wait_for_socket_ready(int sockfd, bool for_read, bool for_write, uint32_t timeout_ms);

/* Context initialization flag */
static bool g_network_initialized = false;

/* Point-free style function types */
typedef void (*SocketOperation)(int sockfd);
typedef bool (*SocketPredicate)(int sockfd);
typedef void (*ClientOperation)(NetClient* client);
typedef bool (*ClientPredicate)(const NetClient* client);
typedef void (*EndpointOperation)(NetEndpoint* endpoint);
typedef bool (*EndpointPredicate)(const NetEndpoint* endpoint);

/**
 * @brief Set socket to non-blocking mode
 * @param sockfd Socket file descriptor
 * @return 0 on success, -1 on failure
 */
static int set_socket_nonblocking(int sockfd) {
#ifdef _WIN32
    u_long mode = 1; /* 1 for non-blocking, 0 for blocking */
    return ioctlsocket(sockfd, FIONBIO, &mode);
#else
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
#endif
}

/**
 * @brief Close socket with proper cleanup
 * @param sockfd Socket file descriptor
 */
static void close_socket(int sockfd) {
    if (sockfd <= 0) return;
    
    /* Set linger to ensure complete socket shutdown */
    struct linger ling = {1, 0}; /* Immediate shutdown */
    setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (sock_opt_type)&ling, sizeof(ling));
    
    /* Shutdown both directions */
    shutdown(sockfd, SHUT_RDWR);
    
    /* Close socket */
    close(sockfd);
}

#ifdef _WIN32
/**
 * @brief Initialize Winsock subsystem
 * @return true if successful, false otherwise
 */
static bool init_winsock(void) {
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}

/**
 * @brief Cleanup Winsock subsystem
 */
static void cleanup_winsock(void) {
    WSACleanup();
}
#endif

/**
 * @brief Get last socket error code
 * @return Error code
 */
static int get_last_socket_error(void) {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

/**
 * @brief Convert socket error code to string
 * @param error_code Error code
 * @return Error string
 */
static const char* socket_error_to_string(int error_code) {
#ifdef _WIN32
    static char error_buffer[256];
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)error_buffer,
        sizeof(error_buffer),
        NULL
    );
    return error_buffer;
#else
    return strerror(error_code);
#endif
}

/**
 * @brief Get current time in milliseconds
 * @return Current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart / 10000; /* Convert to milliseconds */
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

/**
 * @brief Wait for socket to be ready for read/write
 * @param sockfd Socket file descriptor
 * @param for_read Check for read readiness if true
 * @param for_write Check for write readiness if true
 * @param timeout_ms Timeout in milliseconds
 * @return true if socket is ready, false otherwise
 */
static bool wait_for_socket_ready(int sockfd, bool for_read, bool for_write, uint32_t timeout_ms) {
    fd_set readfds, writefds;
    struct timeval tv;
    
    /* Initialize file descriptor sets */
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    
    if (for_read) {
        FD_SET(sockfd, &readfds);
    }
    
    if (for_write) {
        FD_SET(sockfd, &writefds);
    }
    
    /* Set timeout */
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    /* Wait for socket to be ready */
    int ret = select(sockfd + 1, 
                    for_read ? &readfds : NULL,
                    for_write ? &writefds : NULL,
                    NULL, &tv);
    
    if (ret <= 0) {
        return false; /* Timeout or error */
    }
    
    /* Check if socket is ready */
    if (for_read && !FD_ISSET(sockfd, &readfds)) {
        return false;
    }
    
    if (for_write && !FD_ISSET(sockfd, &writefds)) {
        return false;
    }
    
    return true;
}


// Initialize the network context
bool polycall_net_init(polycall_network_context_t* context) {
    if (!context) return false;
    memset(context, 0, sizeof(polycall_network_context_t));
    pthread_mutex_init(&context->clients_lock, NULL);
    context->initialized = true;
    return true;
}

// Close the network context
void polycall_net_close(polycall_network_context_t* context) {
    if (!context || !context->initialized) return;
    pthread_mutex_destroy(&context->clients_lock);
    context->initialized = false;
}

// Send data to a specific client
ssize_t polycall_net_send(polycall_network_context_t* context, uint32_t client_id, const void* data, size_t size) {
    if (!context || !context->initialized || !data || size == 0) return -1;
    
    for (int i = 0; i < POLYCALL_NET_MAX_CLIENTS; i++) {
        if (context->clients[i].id == client_id) {
            return send(context->clients[i].socket.fd, data, size, 0);
        }
    }
    return -1;
}

// Receive data from a specific client
ssize_t polycall_net_receive(polycall_network_context_t* context, uint32_t client_id, void* buffer, size_t size) {
    if (!context || !context->initialized || !buffer || size == 0) return -1;
    
    for (int i = 0; i < POLYCALL_NET_MAX_CLIENTS; i++) {
        if (context->clients[i].id == client_id) {
            return recv(context->clients[i].socket.fd, buffer, size, 0);
        }
    }
    return -1;
}

/**
 * @brief Find available socket in the client array
 * @param context Network context
 * @return Available socket or NULL if none available
 */
static NetSocket* find_available_socket(polycall_network_context_t* context) {
    if (!context) return NULL;
    
    /* Find inactive client */
    for (uint32_t i = 0; i < NET_MAX_CLIENTS; i++) {
        if (!(context->active_clients & (1U << i))) {
            context->active_clients |= (1U << i);
            context->client_count++;
            return &context->clients[i].socket;
        }
    }
    
    return NULL;
}

/**
 * @brief Initialize network subsystem
 * @return true if successful, false otherwise
 */
bool net_initialize(void) {
    if (g_network_initialized) {
        return true; /* Already initialized */
    }
    
#ifdef _WIN32
    if (!init_winsock()) {
        return false;
    }
#endif
    
    g_network_initialized = true;
    return true;
}

/**
 * @brief Shutdown network subsystem
 */
void net_shutdown(void) {
    if (!g_network_initialized) {
        return; /* Not initialized */
    }
    
#ifdef _WIN32
    cleanup_winsock();
#endif
    
    g_network_initialized = false;
}

/**
 * @brief Create network context
 * @return Network context or NULL on failure
 */
polycall_network_context_t* net_create_context(void) {
    if (!g_network_initialized) {
        if (!net_initialize()) {
            return NULL;
        }
    }
    
    polycall_network_context_t* context = calloc(1, sizeof(polycall_network_context_t));
    if (!context) {
        return NULL;
    }
    
    context->initialized = true;
    context->next_client_id = 1;
    context->next_endpoint_id = 1;
    
    return context;
}

/**
 * @brief Destroy network context
 * @param context Network context
 */
void net_destroy_context(polycall_network_context_t* context) {
    if (!context || !context->initialized) {
        return;
    }
    
    /* Close all endpoints */
    for (uint32_t i = 0; i < NET_MAX_ENDPOINTS; i++) {
        if (context->active_endpoints & (1U << i)) {
            net_close_endpoint(context, context->endpoints[i].id);
        }
    }
    
    /* Close all clients */
    for (uint32_t i = 0; i < NET_MAX_CLIENTS; i++) {
        if (context->active_clients & (1U << i)) {
            net_close_client(context, context->clients[i].id);
        }
    }
    
    context->initialized = false;
    free(context);
}

/**
 * @brief Create network endpoint
 * @param context Network context
 * @param protocol Protocol type
 * @param port Port number
 * @param flags Endpoint flags
 * @return Endpoint ID or 0 on failure
 */
uint32_t net_create_endpoint(
    polycall_network_context_t* context,
    NetProtocol protocol,
    uint16_t port,
    uint32_t flags
) {
    if (!context || !context->initialized) {
        return 0;
    }
    
    /* Find inactive endpoint */
    uint32_t endpoint_index = 0;
    while (endpoint_index < NET_MAX_ENDPOINTS && 
          (context->active_endpoints & (1U << endpoint_index))) {
        endpoint_index++;
    }
    
    if (endpoint_index >= NET_MAX_ENDPOINTS) {
        context->last_error = NET_ERROR_INVALID;
        if (context->handlers.on_error) {
            context->handlers.on_error(NET_ERROR_INVALID, 
                                      "Maximum number of endpoints reached",
                                      context->user_data);
        }
        return 0;
    }
    
    /* Initialize endpoint */
    NetEndpoint* endpoint = &context->endpoints[endpoint_index];
    memset(endpoint, 0, sizeof(NetEndpoint));
    
    endpoint->id = context->next_endpoint_id++;
    endpoint->port = port;
    
    /* Create socket */
    endpoint->socket.protocol = protocol;
    endpoint->socket.role = NET_ROLE_SERVER;
    endpoint->socket.flags = flags;
    endpoint->socket.fd = socket(AF_INET, 
                               protocol == NET_PROTOCOL_TCP ? SOCK_STREAM : SOCK_DGRAM,
                               0);
    
    if (endpoint->socket.fd < 0) {
        context->last_error = NET_ERROR_SOCKET;
        if (context->handlers.on_error) {
            context->handlers.on_error(NET_ERROR_SOCKET, 
                                      ERROR_SOCKET_CREATION,
                                      context->user_data);
        }
        return 0;
    }
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(endpoint->socket.fd, SOL_SOCKET, SO_REUSEADDR,
                  (sock_opt_type)&opt, sizeof(opt)) < 0) {
        close_socket(endpoint->socket.fd);
        context->last_error = NET_ERROR_SOCKET;
        if (context->handlers.on_error) {
            context->handlers.on_error(NET_ERROR_SOCKET, 
                                      ERROR_SOCKET_OPTION,
                                      context->user_data);
        }
        return 0;
    }
    
    /* Set non-blocking mode if requested */
    if (flags & NET_FLAG_NONBLOCKING) {
        if (set_socket_nonblocking(endpoint->socket.fd) < 0) {
            close_socket(endpoint->socket.fd);
            context->last_error = NET_ERROR_SOCKET;
            if (context->handlers.on_error) {
                context->handlers.on_error(NET_ERROR_SOCKET, 
                                         "Failed to set non-blocking mode",
                                         context->user_data);
            }
            return 0;
        }
    }
    
    /* Mark endpoint as active */
    context->active_endpoints |= (1U << endpoint_index);
    context->endpoint_count++;
    
    /* Don't bind yet, that's done in listen */
    endpoint->socket.state = NET_STATE_INITIALIZED;
    
    return endpoint->id;
}

/**
 * @brief Start listening on endpoint
 * @param context Network context
 * @param endpoint_id Endpoint ID
 * @param backlog Connection backlog
 * @return Error code
 */
NetError net_listen(
    polycall_network_context_t* context,
    uint32_t endpoint_id,
    uint16_t backlog
) {
    if (!context || !context->initialized) {
        return NET_ERROR_NOT_INITIALIZED;
    }
    
    /* Find endpoint */
    NetEndpoint* endpoint = NULL;
    uint32_t endpoint_index = 0;
    
    for (uint32_t i = 0; i < NET_MAX_ENDPOINTS; i++) {
        if ((context->active_endpoints & (1U << i)) && 
            context->endpoints[i].id == endpoint_id) {
            endpoint = &context->endpoints[i];
            endpoint_index = i;
            break;
        }
    }
    
    if (!endpoint) {
        return NET_ERROR_INVALID;
    }
    
    /* Check if endpoint is already listening */
    if (endpoint->socket.state == NET_STATE_LISTENING) {
        return NET_SUCCESS;
    }
    
    /* Bind to address */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint->port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(endpoint->socket.fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        context->last_error = NET_ERROR_BIND;
        if (context->handlers.on_error) {
            context->handlers.on_error(NET_ERROR_BIND, 
                                      ERROR_ADDRESS_BINDING,
                                      context->user_data);
        }
        
        /* Check if port is in use */
        if (get_last_socket_error() == EADDRINUSE) {
            context->last_error = NET_ERROR_BIND;
            if (context->handlers.on_error) {
                context->handlers.on_error(NET_ERROR_BIND, 
                                         ERROR_PORT_IN_USE,
                                         context->user_data);
            }
        }
        
        return NET_ERROR_BIND;
    }
    
    /* Store local address */
    endpoint->socket.local.port = endpoint->port;
    endpoint->socket.local.ip = INADDR_ANY;
    
    /* For TCP, start listening */
    if (endpoint->socket.protocol == NET_PROTOCOL_TCP) {
        if (listen(endpoint->socket.fd, backlog > 0 ? backlog : NETWORK_DEFAULT_BACKLOG) < 0) {
            context->last_error = NET_ERROR_LISTEN;
            if (context->handlers.on_error) {
                context->handlers.on_error(NET_ERROR_LISTEN, 
                                         ERROR_LISTEN_FAILED,
                                         context->user_data);
            }
            return NET_ERROR_LISTEN;
        }
    }
    
    /* Update endpoint state */
    endpoint->socket.state = NET_STATE_LISTENING;
    endpoint->backlog = backlog > 0 ? backlog : NETWORK_DEFAULT_BACKLOG;
    endpoint->is_active = true;
    
    return NET_SUCCESS;
}

/**
 * @brief Connect to remote host
 * @param context Network context
 * @param protocol Protocol type
 * @param host Remote host
 * @param port Remote port
 * @param flags Connection flags
 * @param timeout_ms Connection timeout in milliseconds
 * @return Client ID or 0 on failure
 */
uint32_t net_connect(
    polycall_network_context_t* context,
    NetProtocol protocol,
    const char* host,
    uint16_t port,
    uint32_t flags,
    uint32_t timeout_ms
) {
    if (!context || !context->initialized || !host || port == 0) {
        return 0;
    }
    
    /* Find available client slot */
    NetSocket* socket = find_available_socket(context);
    if (!socket) {
        context->last_error = NET_ERROR_INVALID;
        if (context->handlers.on_error) {
            context->handlers.on_error(NET_ERROR_INVALID, 
                                     "Maximum number of clients reached",
                                     context->user_data);
        }
        return 0;
    }
    
    /* Get client from socket pointer */
    NetClient* client = (NetClient*)((char*)socket - offsetof(NetClient, socket));
    uint32_t client_id = context->next_client_id++;
    client->id = client_id;
    client->is_active = true;
    
    /* Create socket */
    socket->fd = socket(AF_INET, 
                      protocol == NET_PROTOCOL_TCP ? SOCK_STREAM : SOCK_DGRAM,
                      0);
    
    if (socket->fd < 0) {
        client->is_active = false;
        context->last_error = NET_ERROR_SOCKET;
        if (context->handlers.on_error) {
            context->handlers.on_error(NET_ERROR_SOCKET, 
                                     ERROR_SOCKET_CREATION,
                                     context->user_data);
        }
        return 0;
    }
    
    /* Initialize socket */
    socket->protocol = protocol;
    socket->role = NET_ROLE_CLIENT;
    socket->flags = flags;
    socket->state = NET_STATE_INITIALIZED;
    
    /* Set non-blocking mode if requested */
    if (flags & NET_FLAG_NONBLOCKING) {
        if (set_socket_nonblocking(socket->fd) < 0) {
            close_socket(socket->fd);
            client->is_active = false;
            context->last_error = NET_ERROR_SOCKET;
            if (context->handlers.on_error) {
                context->handlers.on_error(NET_ERROR_SOCKET, 
                                        "Failed to set non-blocking mode",
                                        context->user_data);
            }
            return 0;
        }
    }
    
    /* Resolve remote host */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    /* Try to convert IP address string */
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        /* Not a valid IP address, try hostname resolution */
        struct hostent* he = gethostbyname(host);
        if (!he) {
            close_socket(socket->fd);
            client->is_active = false;
            context->last_error = NET_ERROR_INVALID;
            if (context->handlers.on_error) {
                context->handlers.on_error(NET_ERROR_INVALID, 
                                        "Failed to resolve hostname",
                                        context->user_data);
            }
            return 0;
        }
        
        /* Copy IP address */
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    /* Store remote address */
    socket->remote.ip = addr.sin_addr.s_addr;
    socket->remote.port = port;
    strncpy(socket->remote.host, host, sizeof(socket->remote.host) - 1);
    
    /* For UDP, we don't need to connect */
    if (protocol == NET_PROTOCOL_UDP) {
        socket->state = NET_STATE_CONNECTED;
        socket->last_activity = get_current_time_ms();
        return client_id;
    }
    
    /* For TCP, connect to remote host */
    socket->state = NET_STATE_CONNECTING;
    
    int ret = connect(socket->fd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        int error = get_last_socket_error();
        
#ifdef _WIN32
        bool is_would_block = (error == WSAEWOULDBLOCK);
#else
        bool is_would_block = (error == EINPROGRESS || error == EWOULDBLOCK);
#endif
        
        if (!is_would_block || !(flags & NET_FLAG_NONBLOCKING)) {
            close_socket(socket->fd);
            client->is_active = false;
            context->last_error = NET_ERROR_CONNECT;
            if (context->handlers.on_error) {
                context->handlers.on_error(NET_ERROR_CONNECT, 
                                        ERROR_CONNECTION_FAILED,
                                        context->user_data);
            }
            return 0;
        }
        
        /* For non-blocking sockets, wait for connection */
        if (!wait_for_socket_ready(socket->fd, false, true, 
                                 timeout_ms > 0 ? timeout_ms : NETWORK_DEFAULT_TIMEOUT_MS)) {
            close_socket(socket->fd);
            client->is_active = false;
            context->last_error = NET_ERROR_TIMEOUT;
            if (context->handlers.on_error) {
                context->handlers.on_error(NET_ERROR_TIMEOUT, 
                                        ERROR_TIMEOUT,
                                        context->user_data);
            }
            return 0;
        }
        
        /* Check for connection error */
        int err = 0;
        socklen_t err_len = sizeof(err);
        if (getsockopt(socket->fd, SOL_SOCKET, SO_ERROR, (sock_opt_type)&err, &err_len) < 0 || err != 0) {
            close_socket(socket->fd);
            client->is_active = false;
            context->last_error = NET_ERROR_CONNECT;
            if (context->handlers.on_error) {
                context->handlers.on_error(NET_ERROR_CONNECT, 
                                        ERROR_CONNECTION_FAILED,
                                        context->user_data);
            }
            return 0;
        }
    }
    
    /* Connection successful */
    socket->state = NET_STATE_CONNECTED;
    socket->last_activity = get_current_time_ms();
    
    return client_id;
}

/**
 * @brief Send data to client
 * @param context Network context
 * @param client_id Client ID
 * @param data Data buffer
 * @param size Data size
 * @param flags Send flags
 * @return Bytes sent or negative error code
 */
ssize_t net_send(
    polycall_network_context_t* context,
    uint32_t client_id,
    const void* data,
    size_t size,
    uint32_t flags
) {
    if (!context || !context->initialized || !data || size == 0) {
        return NET_ERROR_INVALID;
    }
    
    /* Find client */
    NetClient* client = net_get_client(context, client_id);
    if (!client) {
        return NET_ERROR_INVALID;
    }
    
    /* Check if client is connected */
    if (client->socket.state != NET_STATE_CONNECTED) {
        return NET_ERROR_INVALID;
    }
    
    /* Send data */
    ssize_t sent;
    if (client->socket.protocol == NET_PROTOCOL_TCP) {
        sent = send(client->socket.fd, data, size, flags);
    } else {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(client->socket.remote.port);
        addr.sin_addr.s_addr = client->socket.remote.ip;
        
        sent = sendto(client->socket.fd, data, size, flags,
                     (struct sockaddr*)&addr, sizeof(addr));
    }
    
    if (sent < 0) {
        int error = get_last_socket_error();
        
#ifdef _WIN32
        bool is_would_block = (error == WSAEWOULDBLOCK);
#else
        bool is_would_block = (error == EWOULDBLOCK || error == EAGAIN);
#endif
        
        if (is_would_block) {
            return NET_ERROR_WOULD_BLOCK;
        }
        
        return NET_ERROR_SEND;
    }
    
    /* Update last activity */
    client->socket.last_activity = get_current_time_ms();
    
    return sent;
}

/**
 * @brief Receive data from client
 * @param context Network context
 * @param client_id Client ID
 * @param data Data buffer
 * @param size Buffer size
 * @param flags Receive flags
 * @return Bytes received or negative error code
 */
ssize_t net_receive(
    polycall_network_context_t* context,
    uint32_t client_id,
    void* data,
    size_t size,
    uint32_t flags
) {
    if (!context || !context->initialized || !data || size == 0) {
        return NET_ERROR_INVALID;
    }
    
    /* Find client */
    NetClient* client = net_get_client(context, client_id);
    if (!client) {
        return NET_ERROR_INVALID;
    }
    
    /* Check if client is connected */
    if (client->socket.state != NET_STATE_CONNECTED) {
        return NET_ERROR_INVALID;
    }
    
    /* Receive data */
    ssize_t received;
    if (client->socket.protocol == NET_PROTOCOL_TCP) {
        received = recv(client->socket.fd, data, size, flags);
    } else {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        
        received = recvfrom(client->socket.fd, data, size, flags,
                          (struct sockaddr*)&addr, &addr_len);
        
        /* Update remote address for UDP */
        if (received > 0) {
            client->socket.remote.ip = addr.sin_addr.s_addr;
            client->socket.remote.port = ntohs(addr.sin_port);
        }
    }
    
    if (received < 0) {
        int error = get_last_socket_error();
        
#ifdef _WIN32
        bool is_would_block = (error == WSAEWOULDBLOCK);
#else
        bool is_would_block = (error == EWOULDBLOCK || error == EAGAIN);
#endif
        
        if (is_would_block) {
            return NET_ERROR_WOULD_BLOCK;
        }
        
        return NET_ERROR_RECEIVE;
    }
    
    /* Update last activity */
    client->socket.last_activity = get_current_time_ms();
    
    return received;
}

/**
 * @brief Batch send data to multiple clients
 * @param context Network context
 * @param client_ids Array of client IDs
 * @param count Number of clients
 * @param data Data buffer
 * @param size Data size
 * @param flags Send flags
 * @return Number of successful sends
 */
size_t net_batch_send(
    polycall_network_context_t* context,
    const uint32_t* client_ids,
    size_t count,
    const void* data,
    size_t size,
    uint32_t flags
) {
    if (!context || !context->initialized || !client_ids || !data || size == 0 || count == 0) {
        return 0;
    }
    
    size_t success_count = 0;
    
    /* Send to each client */
    for (size_t i = 0; i < count; i++) {
        if (net_send(context, client_ids[i], data, size, flags) >= 0) {
            success_count++;
        }
    }
    
    return success_count;
}

/**
 * @brief Process network events
 * @param context Network context
 * @param timeout_ms Timeout in milliseconds
 * @return Error code
 */
NetError net_process_events(
    polycall_network_context_t* context,
    uint32_t timeout_ms
) {
    if (!context || !context->initialized) {
        return NET_ERROR_NOT_INITIALIZED;
    }
    
    /* Initialize file descriptor sets */
    fd_set readfds, writefds, exceptfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    
    int max_fd = -1;
    
    /* Add listening endpoints to read set */
    for (uint32_t i = 0; i < NET_MAX_ENDPOINTS; i++) {
        if (context->active_endpoints & (1U << i)) {
            NetEndpoint* endpoint = &context->endpoints[i];
            
            if (endpoint->socket.state == NET_STATE_LISTENING) {
                FD_SET(endpoint->socket.fd, &readfds);
                if (endpoint->socket.fd > max_fd) {
                    max_fd = endpoint->socket.fd;
                }
            }
        }
    }
    
    /* Add connected clients to read/except sets */
    for (uint32_t i = 0; i < NET_MAX_CLIENTS; i++) {
        if (context->active_clients & (1U << i)) {
            NetClient* client = &context->clients[i];
            
            if (client->socket.state == NET_STATE_CONNECTED) {
                FD_SET(client->socket.fd, &readfds);
                FD_SET(client->socket.fd, &exceptfds);
                if (client->socket.fd > max_fd) {
                    max_fd = client->socket.fd;
                }
            }
        }
    }
    
    /* If no active sockets, return */
    if (max_fd < 0) {
        return NET_SUCCESS;
    }
    
    /* Set timeout */
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    /* Wait for events */
    int result = select(max_fd + 1, &readfds, &writefds, &exceptfds, &tv);
    
    if (result < 0) {
        context->last_error = NET_ERROR_INVALID;
        return NET_ERROR_INVALID;
    }
    
    if (result == 0) {
        /* Timeout, no events */
        return NET_SUCCESS;
    }
    
    /* Process endpoint events first (new connections) */
    for (uint32_t i = 0; i < NET_MAX_ENDPOINTS; i++) {
        if (context->active_endpoints & (1U << i)) {
            NetEndpoint* endpoint = &context->endpoints[i];
            
            if (endpoint->socket.state == NET_STATE_LISTENING && 
                FD_ISSET(endpoint->socket.fd, &readfds)) {
                
                /* Accept new connection */
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                
                int client_fd = accept(endpoint->socket.fd, 
                                     (struct sockaddr*)&client_addr, 
                                     &addr_len);
                
                if (client_fd < 0) {
                    /* Failed to accept */
                    continue;
                }
                
                /* Find available client slot */
                NetSocket* socket = find_available_socket(context);
                if (!socket) {
                    /* No available slots, close connection */
                    close_socket(client_fd);
                    continue;
                }
                
                /* Get client from socket pointer */
                NetClient* client = (NetClient*)((char*)socket - offsetof(NetClient, socket));
                uint32_t client_id = context->next_client_id++;
                client->id = client_id;
                client->is_active = true;
                
                /* Initialize client socket */
                socket->fd = client_fd;
                socket->protocol = endpoint->socket.protocol;
                socket->role = NET_ROLE_CLIENT;
                socket->flags = endpoint->socket.flags;
                socket->state = NET_STATE_CONNECTED;
                socket->last_activity = get_current_time_ms();
                
                /* Set non-blocking mode if endpoint is non-blocking */
                if (endpoint->socket.flags & NET_FLAG_NONBLOCKING) {
                    set_socket_nonblocking(client_fd);
                }
                
                /* Store remote address */
                socket->remote.ip = client_addr.sin_addr.s_addr;
                socket->remote.port = ntohs(client_addr.sin_port);
                inet_ntop(AF_INET, &client_addr.sin_addr, 
                         socket->remote.host, sizeof(socket->remote.host));
                
                /* Store local address */
                struct sockaddr_in local_addr;
                socklen_t local_addr_len = sizeof(local_addr);
                getsockname(client_fd, (struct sockaddr*)&local_addr, &local_addr_len);
                socket->local.ip = local_addr.sin_addr.s_addr;
                socket->local.port = ntohs(local_addr.sin_port);
                
                /* Notify connection event */
                if (context->handlers.on_connect) {
                    context->handlers.on_connect(client_id, context->user_data);
                }
            }
        }
    }
    
    /* Process client events */
    for (uint32_t i = 0; i < NET_MAX_CLIENTS; i++) {
        if (context->active_clients & (1U << i)) {
            NetClient* client = &context->clients[i];
            
            if (client->socket.state == NET_STATE_CONNECTED) {
                
                /* Check for error events */
                if (FD_ISSET(client->socket.fd, &exceptfds)) {
                    /* Socket error */
                    if (context->handlers.on_disconnect) {
                        context->handlers.on_disconnect(client->id, context->user_data);
                    }
                    
                    /* Close connection */
                    close_socket(client->socket.fd);
                    client->is_active = false;
                    client->socket.state = NET_STATE_CLOSED;
                    client->socket.fd = -1;
                    
                    /* Update active clients bitmap */
                    context->active_clients &= ~(1U << i);
                    context->client_count--;
                    
                    continue;
                }
                
                /* Check for read events */
                if (FD_ISSET(client->socket.fd, &readfds)) {
                    /* Data available */
                    uint8_t buffer[NETWORK_MAX_BUFFER_SIZE];
                    ssize_t received;
                    
                    if (client->socket.protocol == NET_PROTOCOL_TCP) {
                        received = recv(client->socket.fd, buffer, 
                                     sizeof(buffer), 0);
                    } else {
                        struct sockaddr_in addr;
                        socklen_t addr_len = sizeof(addr);
                        
                        received = recvfrom(client->socket.fd, buffer, 
                                         sizeof(buffer), 0,
                                         (struct sockaddr*)&addr, &addr_len);
                        
                        /* Update remote address for UDP */
                        if (received > 0) {
                            client->socket.remote.ip = addr.sin_addr.s_addr;
                            client->socket.remote.port = ntohs(addr.sin_port);
                        }
                    }
                    
                    if (received <= 0) {
                        /* Connection closed or error */
                        if (context->handlers.on_disconnect) {
                            context->handlers.on_disconnect(client->id, context->user_data);
                        }
                        
                        /* Close connection */
                        close_socket(client->socket.fd);
                        client->is_active = false;
                        client->socket.state = NET_STATE_CLOSED;
                        client->socket.fd = -1;
                        
                        /* Update active clients bitmap */
                        context->active_clients &= ~(1U << i);
                        context->client_count--;
                        
                        continue;
                    }
                    
                    /* Update last activity */
                    client->socket.last_activity = get_current_time_ms();
                    
                    /* Notify message event */
                    if (context->handlers.on_message) {
                        NetMessage message = {
                            .data = buffer,
                            .size = received,
                            .flags = 0,
                            .address = client->socket.remote
                        };
                        
                        context->handlers.on_message(client->id, &message, context->user_data);
                    }
                }
            }
        }
    }
    
    return NET_SUCCESS;
}