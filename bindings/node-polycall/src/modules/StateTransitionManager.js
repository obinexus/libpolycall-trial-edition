// src/modules/StateTransitionManager.js
const { pipe, curry } = require('../utils');

/**
 * StateTransitionManager provides a robust interface for managing state machine transitions
 * with a data-oriented design pattern and point-free style operations
 */
class StateTransitionManager {
  /**
   * Creates a new StateTransitionManager
   * @param {StateMachine} stateMachine - The state machine to manage
   * @param {Object} options - Configuration options
   */
  constructor(stateMachine, options = {}) {
    this.stateMachine = stateMachine;
    this.options = {
      autoSetupTransitions: true,
      defaultStates: ['INIT', 'READY', 'RUNNING', 'ERROR'],
      allowSelfTransitions: false,
      ...options
    };
    
    this.transitionHistory = [];
    this.maxHistoryLength = options.maxHistoryLength || 100;
    
    // Initialize default states and transitions
    if (this.options.autoSetupTransitions) {
      this.setupDefaultStates();
      this.setupDefaultTransitions();
    }
  }
  
  /**
   * Sets up default states with appropriate handlers
   */
  setupDefaultStates() {
    // Define state handlers using data-oriented approach
    const stateHandlers = {
      INIT: {
        onEnter: () => console.log('State: System initialized')
      },
      READY: {
        onEnter: () => console.log('State: System ready')
      },
      RUNNING: {
        onEnter: () => console.log('State: System running')
      },
      ERROR: {
        onEnter: (error) => console.error('State: System error', error || '')
      }
    };
    
    // Add default states if they don't exist
    this.options.defaultStates.forEach(stateName => {
      try {
        // Check if state already exists
        this.stateMachine.getState(stateName);
      } catch (error) {
        // Add state with appropriate handlers
        const handlers = stateHandlers[stateName] || {};
        this.stateMachine.addState(stateName, handlers);
      }
    });
  }
  
  /**
   * Sets up default transitions between states
   */
  setupDefaultTransitions() {
    // Define transition paths as adjacency graph
    const transitions = [
      { from: 'INIT', to: 'READY' },
      { from: 'READY', to: 'RUNNING' },
      { from: 'READY', to: 'ERROR' },
      { from: 'RUNNING', to: 'READY' },
      { from: 'RUNNING', to: 'ERROR' },
      { from: 'ERROR', to: 'INIT' },
      { from: 'ERROR', to: 'READY' },
      { from: 'INIT', to: 'RUNNING' } // Direct transition from INIT to RUNNING
    ];
    
    // Add transitions using point-free style
    const addTransition = curry((machine, from, to) => {
      try {
        const fromState = machine.getState(from);
        
        // Check if transition already exists
        if (!fromState.canTransitionTo(to)) {
          machine.addTransition(fromState, to, {
            guard: () => true,
            before: () => console.log(`Transitioning from ${from} to ${to}`),
            after: () => console.log(`Completed transition to ${to}`)
          });
        }
      } catch (error) {
        console.warn(`Failed to add transition ${from} -> ${to}: ${error.message}`);
      }
    });
    
    // Apply all transitions
    transitions.forEach(({ from, to }) => {
      addTransition(this.stateMachine)(from, to);
    });
  }
  
  /**
   * Safely transition to a target state with fallback path finding
   * @param {string} targetState - Target state name
   * @returns {Promise<boolean>} Success status
   */
  async safeTransitionTo(targetState) {
    const currentState = this.stateMachine.getCurrentState();
    
    if (!currentState) {
      console.error('No current state defined');
      return false;
    }
    
    try {
      // Direct transition if possible
      if (currentState.canTransitionTo(targetState)) {
        await this.stateMachine.executeTransition(targetState);
        this.recordTransition(currentState.name, targetState);
        return true;
      }
      
      // Find path for indirect transition
      const path = this.findTransitionPath(currentState.name, targetState);
      
      if (!path || path.length === 0) {
        console.error(`No valid transition path from ${currentState.name} to ${targetState}`);
        return false;
      }
      
      // Execute each transition in the path
      for (const nextState of path) {
        await this.stateMachine.executeTransition(nextState);
        this.recordTransition(currentState.name, nextState);
      }
      
      return true;
    } catch (error) {
      console.error(`Transition error: ${error.message}`);
      
      // Try to transition to ERROR state as fallback
      if (currentState.name !== 'ERROR') {
        try {
          if (currentState.canTransitionTo('ERROR')) {
            await this.stateMachine.executeTransition('ERROR');
            this.recordTransition(currentState.name, 'ERROR');
          } else {
            console.error('Cannot transition to ERROR state');
          }
        } catch (fallbackError) {
          console.error(`Failed to transition to ERROR state: ${fallbackError.message}`);
        }
      }
      
      return false;
    }
  }
  
  /**
   * Find a transition path between states using BFS algorithm
   * @param {string} fromState - Source state name
   * @param {string} toState - Target state name
   * @returns {Array<string>} Sequence of state names to follow
   */
  findTransitionPath(fromState, toState) {
    // If states are the same, no transition needed
    if (fromState === toState) return [];
    
    // Get all state names
    const states = this.stateMachine.getStateNames();
    
    // Build adjacency map of state transitions
    const transitions = {};
    states.forEach(name => {
      transitions[name] = [];
      try {
        const state = this.stateMachine.getState(name);
        states.forEach(targetName => {
          if (state.canTransitionTo(targetName)) {
            transitions[name].push(targetName);
          }
        });
      } catch (error) {
        // Skip if state cannot be retrieved
      }
    });
    
    // Breadth-first search for shortest path
    const queue = [[fromState]];
    const visited = new Set([fromState]);
    
    while (queue.length > 0) {
      const path = queue.shift();
      const currentState = path[path.length - 1];
      
      // Check if we've reached the target state
      if (currentState === toState) {
        return path.slice(1); // Return path without starting state
      }
      
      // Explore all possible next states
      for (const nextState of transitions[currentState] || []) {
        if (!visited.has(nextState)) {
          visited.add(nextState);
          queue.push([...path, nextState]);
        }
      }
    }
    
    // No path found
    return null;
  }
  
  /**
   * Record a state transition in history
   * @param {string} fromState - Source state
   * @param {string} toState - Target state
   */
  recordTransition(fromState, toState) {
    this.transitionHistory.push({
      timestamp: Date.now(),
      from: fromState,
      to: toState
    });
    
    // Trim history if it exceeds maximum length
    if (this.transitionHistory.length > this.maxHistoryLength) {
      this.transitionHistory.shift();
    }
  }
  
  /**
   * Get all possible transitions from current state
   * @returns {Array<string>} List of possible target states
   */
  getPossibleTransitions() {
    const currentState = this.stateMachine.getCurrentState();
    if (!currentState) return [];
    
    return this.stateMachine.getStateNames()
      .filter(name => currentState.canTransitionTo(name));
  }
  
  /**
   * Get transition history
   * @param {number} limit - Maximum number of entries to return
   * @returns {Array<Object>} Transition history
   */
  getTransitionHistory(limit = this.maxHistoryLength) {
    return this.transitionHistory
      .slice(-Math.min(limit, this.transitionHistory.length));
  }
  
  /**
   * Get current state information
   * @returns {Object} Current state details
   */
  getCurrentStateInfo() {
    const currentState = this.stateMachine.getCurrentState();
    if (!currentState) return null;
    
    return {
      name: currentState.name,
      isLocked: currentState.isLocked,
      possibleTransitions: this.getPossibleTransitions(),
      metadata: Object.fromEntries(currentState.metadata || new Map())
    };
  }
  
  /**
   * Initialize from configuration
   * @param {Object} config - Configuration object
   */
  initializeFromConfig(config) {
    if (!config) return;
    
    // Configure states
    if (config.states && Array.isArray(config.states)) {
      config.states.forEach(stateConfig => {
        try {
          this.stateMachine.getState(stateConfig.name);
        } catch (error) {
          // Add missing state
          this.stateMachine.addState(stateConfig.name, stateConfig.handlers || {});
        }
      });
    }
    
    // Configure transitions
    if (config.transitions && Array.isArray(config.transitions)) {
      config.transitions.forEach(transConfig => {
        try {
          const fromState = this.stateMachine.getState(transConfig.from);
          if (!fromState.canTransitionTo(transConfig.to)) {
            this.stateMachine.addTransition(
              fromState, 
              transConfig.to, 
              transConfig.options || {}
            );
          }
        } catch (error) {
          console.warn(`Failed to configure transition: ${error.message}`);
        }
      });
    }
    
    // Set initial state if specified
    if (config.initialState) {
      try {
        const initialState = this.stateMachine.getState(config.initialState);
        this.stateMachine.currentState = initialState;
      } catch (error) {
        console.warn(`Failed to set initial state: ${error.message}`);
      }
    }
  }
}

module.exports = {
  StateTransitionManager,
  createStateTransitionManager: (stateMachine, options) => 
    new StateTransitionManager(stateMachine, options)
};