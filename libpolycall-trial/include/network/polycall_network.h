#ifndef POLYCALL_NETWORK_H
#define POLYCALL_NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// [Previous constants remain the same]

// Network Address Structure
typedef struct {
    char host[INET_ADDRSTRLEN];
    uint32_t ip;
    uint16_t port;
} polycall_network_address_t;

// Network Socket Structure
typedef struct {
    int fd;
    int state;
    NetworkProtocol protocol;  // Updated to use NetworkProtocol
    polycall_network_address_t local;
    polycall_network_address_t remote;
} polycall_network_socket_t;

// [Rest of the structures and function declarations remain the same]

#ifdef __cplusplus
}
#endif

#endif // POLYCALL_NETWORK_H
