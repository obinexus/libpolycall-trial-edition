# PolyCall CLI Documentation

## Overview
PolyCall CLI is a command-line interface for managing polymorphic function calls and network communications. This tool provides functionality for state machine management, network operations, and runtime configuration.

## Version
Current Version: 1.0.0

## Commands

### Network Commands
- `start_network` - Initializes and starts network services
- `stop_network` - Stops all network services and closes connections
- `list_endpoints` - Displays all configured network endpoints
- `list_clients` - Shows currently connected clients

### State Machine Commands
- `init` - Initialize the state machine system
- `add_state NAME` - Creates a new state with the specified name
- `add_transition NAME FROM TO` - Defines a transition between states
- `execute NAME` - Triggers a state transition

### Miscellaneous
- `help` - Displays this help message
- `quit` - Exits the CLI application

## Usage Examples
