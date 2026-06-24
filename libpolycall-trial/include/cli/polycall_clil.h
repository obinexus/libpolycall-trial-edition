// polycall_cli.h
#ifndef POLYCALL_CLI_H
#define POLYCALL_CLI_H

#include <stdbool.h>
#include <stdint.h>
#include "core/polycall.h"
#include "network/network.h"

#ifdef __cplusplus
extern "C" {
#endif

// CLI command handler function type
typedef bool (*CommandHandler)(const void* runtime, const char* arg1, const char* arg2, const char* arg3);

// Command structure
typedef struct {
    const char* name;
    CommandHandler handler;
    const char* description;
    const char* usage;
} Command;

// Runtime configuration structure
typedef struct {
    bool interactive_mode;
    const char* config_file;
    // Add other configuration fields as needed
} RuntimeConfig;

// PPI Runtime context
typedef struct {
    NetworkProgram* programs[MAX_PROGRAMS];
    size_t program_count;
    polycall_context_t pc_ctx;
    PolyCall_StateMachine* state_machine;
    char command_history[HISTORY_SIZE][MAX_INPUT];
    int history_count;
    PolyCall_StateSnapshot snapshots[POLYCALL_MAX_STATES];
    bool has_snapshot[POLYCALL_MAX_STATES];
    PortMappingArray port_mappings;
    bool interactive_mode;
#ifdef _WIN32
    bool wsaInitialized;
#endif
    bool running;
} PPI_Runtime;

// CLI initialization and cleanup
bool polycall_cli_init(PPI_Runtime* runtime, int argc, char** argv);
void polycall_cli_cleanup(PPI_Runtime* runtime);

// CLI run modes
int polycall_cli_run_interactive(PPI_Runtime* runtime);
int polycall_cli_run_config(PPI_Runtime* runtime, const char* config_file);

// Command processing
bool polycall_cli_process_command(PPI_Runtime* runtime, const char* input);

// Command history management
void polycall_cli_add_to_history(PPI_Runtime* runtime, const char* command);
void polycall_cli_show_history(const PPI_Runtime* runtime);

// Command handlers declaration
bool cmd_init(const PPI_Runtime* runtime, const char* arg1, const char* arg2, const char* arg3);
bool cmd_add_state(const PPI_Runtime* runtime, const char* name, const char* arg2, const char* arg3);
bool cmd_help(const PPI_Runtime* runtime, const char* arg1, const char* arg2, const char* arg3);
bool cmd_quit(const PPI_Runtime* runtime, const char* arg1, const char* arg2, const char* arg3);

#ifdef __cplusplus
}
#endif

#endif // POLYCALL_CLI_H

// polycall_cli.c
#include "cli/polycall_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

// Constants
#define PPI_VERSION "1.0.0"
#define MAX_INPUT 256 
#define MAX_PORTS 64
#define MAX_PROGRAMS 8
#define HISTORY_SIZE 10

// Global runtime instance for signal handlers
static PPI_Runtime* g_runtime_ptr = NULL;

// Command table
static const Command COMMANDS[] = {
    {"init", cmd_init, "Initialize the state machine", "init"},
    {"add_state", cmd_add_state, "Add a new state", "add_state NAME"},
    {"help", cmd_help, "Show help", "help"},
    {"quit", cmd_quit, "Exit program", "quit"},
    // Add additional commands here
};

// Forward declarations of helper functions
static void register_signal_handlers(void);
static void signal_handler(int signum);
static void cleanup_and_exit(void);

// CLI initialization
bool polycall_cli_init(PPI_Runtime* runtime, int argc, char** argv) {
    if (!runtime) return false;
    
    // Store global pointer for signal handlers
    g_runtime_ptr = runtime;
    
    // Initialize runtime defaults
    memset(runtime, 0, sizeof(PPI_Runtime));
    runtime->running = true;
    
    // Parse command line arguments
    bool non_interactive = false;
    const char* config_file = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            config_file = argv[++i];
            non_interactive = true;
        }
    }
    
    // Initialize PolyCall context
    polycall_config_t config = {
        .flags = 0,
        .memory_pool_size = 1024 * 1024,
        .user_data = NULL
    };

    if (polycall_init_with_config(&runtime->pc_ctx, &config) != POLYCALL_SUCCESS) {
        fprintf(stderr, "Failed to initialize PolyCall context\n");
        return false;
    }
    
    // Initialize platform-specific components
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "Failed to initialize Winsock\n");
        return false;
    }
    runtime->wsaInitialized = true;
#endif
    
    // Set runtime mode
    runtime->interactive_mode = !non_interactive;
    
    // Register signal handlers
    register_signal_handlers();
    
    return true;
}

// CLI cleanup
void polycall_cli_cleanup(PPI_Runtime* runtime) {
    if (!runtime) return;
    
    // Clean up network programs
    for (size_t i = 0; i < runtime->program_count; i++) {
        if (runtime->programs[i]) {
            net_cleanup_program(runtime->programs[i]);
            free(runtime->programs[i]);
            runtime->programs[i] = NULL;
        }
    }
    
    // Clean up state machine
    if (runtime->state_machine) {
        polycall_sm_destroy(runtime->state_machine);
        runtime->state_machine = NULL;
    }
    
    // Clean up PolyCall context
    if (runtime->pc_ctx) {
        polycall_cleanup(runtime->pc_ctx);
        runtime->pc_ctx = NULL;
    }
    
    // Platform-specific cleanup
#ifdef _WIN32
    if (runtime->wsaInitialized) {
        WSACleanup();
        runtime->wsaInitialized = false;
    }
#endif
    
    g_runtime_ptr = NULL;
}

// Run interactive CLI mode
int polycall_cli_run_interactive(PPI_Runtime* runtime) {
    char input[MAX_INPUT];
    printf("PolyCall CLI v%s - Type 'help' for commands\n", PPI_VERSION);

    while (runtime->running) {
        printf("\n> ");
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        // Remove newline character
        input[strcspn(input, "\n")] = 0;
        if (strlen(input) == 0) continue;

        // Add command to history
        polycall_cli_add_to_history(runtime, input);
        
        // Process the command
        polycall_cli_process_command(runtime, input);
    }
    
    return 0;
}

// Run from configuration file
int polycall_cli_run_config(PPI_Runtime* runtime, const char* config_file) {
    if (!runtime || !config_file) return 1;
    
    FILE* fp = fopen(config_file, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open config file: %s\n", config_file);
        return 1;
    }

    char line[MAX_INPUT];
    bool network_started = false;
    uint16_t port_number = 8080; // Default port

    while (fgets(line, sizeof(line), fp)) {
        // Remove newline and whitespace
        char* trimmed = line;
        size_t len = strlen(trimmed);
        while (len > 0 && (trimmed[len-1] == '\n' || trimmed[len-1] == '\r')) {
            trimmed[--len] = '\0';
        }

        // Skip empty lines and comments
        if (len == 0 || trimmed[0] == '#') continue;

        // Process the command
        polycall_cli_process_command(runtime, trimmed);
    }

    fclose(fp);
    
    // Run network event loop if in non-interactive mode
    while (runtime->running) {
        // Process all network programs
        for (size_t i = 0; i < runtime->program_count; i++) {
            NetworkProgram* program = runtime->programs[i];
            if (program) {
                net_run(program);
            }
        }
        // Small sleep to prevent CPU spin
        usleep(1000); // 1ms sleep
    }
    
    return 0;
}

// Process a command
bool polycall_cli_process_command(PPI_Runtime* runtime, const char* input) {
    if (!runtime || !input) return false;

    char* input_copy = strdup(input);
    char* cmd = strtok(input_copy, " ");
    char* arg1 = strtok(NULL, " ");
    char* arg2 = strtok(NULL, " ");
    char* arg3 = strtok(NULL, " ");

    if (!cmd) {
        free(input_copy);
        return false;
    }

    // Special case handling
    if (strcmp(cmd, "start_network") == 0) {
        NetworkProgram* program = calloc(1, sizeof(NetworkProgram));
        if (program) {
            net_init_program(program);
            if (program->endpoints && program->count > 0) {
                program->handlers.on_receive = on_network_receive;
                program->handlers.on_connect = on_network_connect;
                program->handlers.on_disconnect = on_network_disconnect;
                runtime->programs[runtime->program_count++] = program;
                printf("Network services started\n");
            } else {
                free(program);
                printf("Failed to start network services\n");
            }
        }
        free(input_copy);
        return true;
    } else if (strcmp(cmd, "stop_network") == 0) {
        for (size_t i = 0; i < runtime->program_count; i++) {
            if (runtime->programs[i]) {
                net_cleanup_program(runtime->programs[i]);
                free(runtime->programs[i]);
                runtime->programs[i] = NULL;
            }
        }
        runtime->program_count = 0;
        printf("Network services stopped\n");
        free(input_copy);
        return true;
    } else if (strcmp(cmd, "list_endpoints") == 0) {
        list_endpoints(runtime);
        free(input_copy);
        return true;
    } else if (strcmp(cmd, "list_clients") == 0) {
        list_clients(runtime);
        free(input_copy);
        return true;
    } else if (strcmp(cmd, "list_states") == 0) {
        list_states(runtime);
        free(input_copy);
        return true;
    } else if (strcmp(cmd, "list_transitions") == 0) {
        list_transitions(runtime);
        free(input_copy);
        return true;
    } else if (strcmp(cmd, "history") == 0) {
        polycall_cli_show_history(runtime);
        free(input_copy);
        return true;
    } else if (strcmp(cmd, "status") == 0) {
        show_status(runtime);
        free(input_copy);
        return true;
    }

    // Find and execute command from the command table
    for (size_t i = 0; i < sizeof(COMMANDS) / sizeof(COMMANDS[0]); i++) {
        if (strcmp(cmd, COMMANDS[i].name) == 0) {
            bool result = COMMANDS[i].handler(runtime, arg1, arg2, arg3);
            if (!result) {
                printf("Usage: %s\n", COMMANDS[i].usage);
            }
            free(input_copy);
            return result;
        }
    }

    printf("Unknown command. Type 'help' for available commands\n");
    free(input_copy);
    return false;
}

// Command history management
void polycall_cli_add_to_history(PPI_Runtime* runtime, const char* command) {
    if (!runtime || !command) return;
    
    size_t cmd_len = strlen(command);
    
    if (runtime->history_count < HISTORY_SIZE) {
        size_t copy_len = (cmd_len < MAX_INPUT - 1) ? cmd_len : MAX_INPUT - 1;
        memcpy(runtime->command_history[runtime->history_count], command, copy_len);
        runtime->command_history[runtime->history_count][copy_len] = '\0';
        runtime->history_count++;
    } else {
        memmove(&runtime->command_history[0], &runtime->command_history[1], 
                (HISTORY_SIZE - 1) * sizeof(runtime->command_history[0]));
        size_t copy_len = (cmd_len < MAX_INPUT - 1) ? cmd_len : MAX_INPUT - 1;
        memcpy(runtime->command_history[HISTORY_SIZE - 1], command, copy_len);
        runtime->command_history[HISTORY_SIZE - 1][copy_len] = '\0';
    }
}

void polycall_cli_show_history(const PPI_Runtime* runtime) {
    if (!runtime) return;
    
    printf("\nCommand History:\n");
    for (int i = 0; i < runtime->history_count; i++) {
        printf("  %d: %s\n", i + 1, runtime->command_history[i]);
    }
}

// Command handlers implementation
bool cmd_init(const PPI_Runtime* runtime, const char* arg1, const char* arg2, const char* arg3) {
    (void)arg1; (void)arg2; (void)arg3;
    
    if (runtime->state_machine) {
        printf("State machine already initialized\n");
        return false;
    }

    if (polycall_sm_create_with_integrity(runtime->pc_ctx, &g_runtime_ptr->state_machine, NULL) 
        != POLYCALL_SM_SUCCESS) {
        printf("Failed to initialize state machine\n");
        return false;
    }

    // Add default states
    polycall_sm_add_state(g_runtime_ptr->state_machine, "INIT", on_init, NULL, false);
    polycall_sm_add_state(g_runtime_ptr->state_machine, "READY", on_ready, NULL, false);
    polycall_sm_add_state(g_runtime_ptr->state_machine, "RUNNING", on_running, NULL, false);
    polycall_sm_add_state(g_runtime_ptr->state_machine, "PAUSED", on_paused, NULL, false);
    polycall_sm_add_state(g_runtime_ptr->state_machine, "ERROR", on_error, NULL, true);

    printf("State machine initialized successfully\n");
    return true;
}

bool cmd_add_state(const PPI_Runtime* runtime, const char* name, const char* arg2, const char* arg3) {
    (void)arg2; (void)arg3;
    
    if (!runtime->state_machine) {
        printf("State machine not initialized. Use 'init' first.\n");
        return false;
    }

    if (!name) {
        return false;
    }

    if (polycall_sm_add_state(runtime->state_machine, name, NULL, NULL, false) 
        == POLYCALL_SM_SUCCESS) {
        printf("State '%s' added successfully\n", name);
        return true;
    }
    
    printf("Failed to add state\n");
    return false;
}