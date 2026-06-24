#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "cli/polycall_cli.h"
#include "core/polycall.h"
#include "state/polycall_state_machine.h"
#include "network/network.h"
#include "parser/polycall_parser.h"
#include "protocol/polycall_protocol.h"

// External declarations for network callbacks
extern void on_network_receive(NetworkEndpoint* endpoint, NetworkPacket* packet);
extern void on_network_connect(NetworkEndpoint* endpoint);
extern void on_network_disconnect(NetworkEndpoint* endpoint);

// Global runtime instance for signals
extern PPI_Runtime g_runtime;

// State callbacks
static void on_init(polycall_context_t ctx) {
    (void)ctx;
    printf("State callback: System initialized\n");
}

static void on_ready(polycall_context_t ctx) {
    (void)ctx;
    printf("State callback: System ready\n");
}

static void on_running(polycall_context_t ctx) {
    (void)ctx;
    printf("State callback: System running\n");
}

static void on_paused(polycall_context_t ctx) {
    (void)ctx;
    printf("State callback: System paused\n");
}

static void on_error(polycall_context_t ctx) {
    (void)ctx;
    printf("State callback: System error\n");
    g_runtime.running = false;
}

// Helper function implementations
void list_states(void) {
    if (!g_runtime.state_machine) {
        printf("State machine not initialized\n");
        return;
    }

    printf("\nStates:\n");
    for (unsigned int i = 0; i < g_runtime.state_machine->num_states; i++) {
        printf("  %u: %s (locked: %s)\n", 
               i, 
               g_runtime.state_machine->states[i].name, 
               g_runtime.state_machine->states[i].is_locked ? "yes" : "no");
    }
}

void list_transitions(void) {
    if (!g_runtime.state_machine) {
        printf("State machine not initialized\n");
        return;
    }

    printf("\nTransitions:\n");
    for (unsigned int i = 0; i < g_runtime.state_machine->num_transitions; i++) {
        printf("  %s: %u -> %u\n", 
               g_runtime.state_machine->transitions[i].name,
               g_runtime.state_machine->transitions[i].from_state,
               g_runtime.state_machine->transitions[i].to_state);
    }
}

void list_endpoints(void) {
    for (size_t i = 0; i < g_runtime.program_count; i++) {
        NetworkProgram* program = g_runtime.programs[i];
        if (program && program->endpoints) {
            printf("\nProgram %zu Endpoints:\n", i);
            for (size_t j = 0; j < program->count; j++) {
                NetworkEndpoint* ep = &program->endpoints[j];
                printf("  Endpoint %zu: %s:%d (%s)\n",
                       j,
                       ep->address,
                       ep->port,
                       ep->protocol == NET_TCP ? "TCP" : "UDP");
            }
        }
    }
}

void list_clients(void) {
    for (size_t i = 0; i < g_runtime.program_count; i++) {
        NetworkProgram* program = g_runtime.programs[i];
        if (program) {
            printf("\nProgram %zu Clients:\n", i);
            pthread_mutex_lock(&program->clients_lock);
            for (int j = 0; j < NET_MAX_CLIENTS; j++) {
                pthread_mutex_lock(&program->clients[j].lock);
                if (program->clients[j].is_active) {
                    printf("  Client %d: Connected\n", j);
                }
                pthread_mutex_unlock(&program->clients[j].lock);
            }
            pthread_mutex_unlock(&program->clients_lock);
        }
    }
}

void show_status(void) {
    printf("\nSystem Status:\n");
    printf("  State Machine: %s\n", g_runtime.state_machine ? "Initialized" : "Not initialized");
    printf("  Network Programs: %zu\n", g_runtime.program_count);
    printf("  Running: %s\n", g_runtime.running ? "Yes" : "No");
    
    if (g_runtime.state_machine) {
        printf("  Current State: %u\n", g_runtime.state_machine->current_state);
    }
    
    list_endpoints();
    list_clients();
}

// Network callback implementations
void on_network_receive(NetworkEndpoint* endpoint, NetworkPacket* packet) {
    if (!endpoint || !packet || !packet->data) return;
    
    printf("\nReceived data from %s:%d: %.*s\n> ", 
           endpoint->address,
           endpoint->port,
           (int)packet->size, 
           (char*)packet->data);
    fflush(stdout);
    
    // Echo back the received data
    NetworkPacket response = {
        .data = packet->data,
        .size = packet->size,
        .flags = 0
    };
    
    net_send(endpoint, &response);
}

void on_network_connect(NetworkEndpoint* endpoint) {
    if (endpoint) {
        printf("\nNew connection from %s:%d\n> ", 
               endpoint->address, 
               endpoint->port);
        fflush(stdout);
    }
}

void on_network_disconnect(NetworkEndpoint* endpoint) {
    if (endpoint) {
        printf("\nClient disconnected from %s:%d\n> ", 
               endpoint->address, 
               endpoint->port);
        fflush(stdout);
    }
}

// Command handlers
bool cmd_quit(const PPI_Runtime* runtime, const char* arg1, const char* arg2, const char* arg3) {
    (void)runtime; (void)arg1; (void)arg2; (void)arg3;
    g_runtime.running = false;
    return true;
}

bool cmd_help(const PPI_Runtime* runtime, const char* arg1, const char* arg2, const char* arg3) {
    (void)runtime; (void)arg1; (void)arg2; (void)arg3;
    
    printf("\nPolyCall CLI Commands:\n");
    printf("Network Commands:\n");
    printf("  start_network          - Start network services\n");
    printf("  stop_network           - Stop network services\n");
    printf("  list_endpoints         - List all network endpoints\n");
    printf("  list_clients          - List connected clients\n");
    
    printf("\nState Machine Commands:\n");
    printf("  init                  - Initialize the state machine\n");
    printf("  add_state NAME        - Add a new state\n");
    printf("  add_transition NAME FROM TO - Add a transition\n");
    printf("  execute NAME          - Execute a transition\n");
    
    printf("\nMiscellaneous Commands:\n");
    printf("  help                - Show this help message\n");
    printf("  quit                - Exit the program\n");
    
    return true;
}

// Signal handling
static void signal_handler(int signum) {
    (void)signum;
    printf("\nReceived signal, shutting down...\n");
    g_runtime.running = false;
}

static void register_signal_handlers(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

// Directory control and service discovery
bool polycall_init_with_directory(const char* base_path, const char* config_file) {
    printf("Initializing with directory: %s\n", base_path);
    
    // Initialize directory structure
    char command[512];
    snprintf(command, sizeof(command), "mkdir -p %s/{node,python,java,go}", base_path);
    system(command);
    
    // Process configuration file if provided
    if (config_file) {
        FILE* fp = fopen(config_file, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                // Process configuration lines
                // This is a simplified implementation
                printf("Config: %s", line);
            }
            fclose(fp);
        } else {
            fprintf(stderr, "Warning: Could not open config file %s\n", config_file);
        }
    }
    
    // Create a network program
    NetworkProgram* program = calloc(1, sizeof(NetworkProgram));
    if (program) {
        net_init_program(program);
        if (program->endpoints && program->count > 0) {
            program->handlers.on_receive = on_network_receive;
            program->handlers.on_connect = on_network_connect;
            program->handlers.on_disconnect = on_network_disconnect;
            g_runtime.programs[g_runtime.program_count++] = program;
            return true;
        }
        free(program);
    }
    
    return false;
}

// Main entry point
int main(int argc, char* argv[]) {
    // Initialize runtime
    if (!polycall_cli_init(&g_runtime, argc, argv)) {
        fprintf(stderr, "Failed to initialize runtime\n");
        return 1;
    }
    
    int result;
    if (g_runtime.interactive_mode) {
        // Run in interactive mode
        result = polycall_cli_run_interactive(&g_runtime);
    } else {
        // Find config file from arguments
        const char* config_file = NULL;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
                config_file = argv[++i];
                break;
            }
        }
        
        // Run in config mode
        if (config_file) {
            result = polycall_cli_run_config(&g_runtime, config_file);
        } else {
            fprintf(stderr, "No configuration file specified\n");
            result = 1;
        }
    }
    
    // Clean up
    polycall_cli_cleanup(&g_runtime);
    
    return result;
}