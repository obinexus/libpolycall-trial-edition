// server_with_config.js
const http = require('http');
const path = require('path');
const fs = require('fs').promises;
const Router = require('../src/modules/Router');
const StateMachine = require('../src/modules/StateMachine');
const State = require('../src/modules/State');
const { StateTransitionManager } = require('../src/modules/StateTransitionManager');
const ConfigManager = require('../src/parser/ConfigManager');
const { pipe, map } = require('../src/utils');

// Create server instance with proper state management
const createServer = async (configPath = '.polycallrc') => {
  // Initialize components
  const configManager = new ConfigManager();
  const stateMachine = new StateMachine({
    allowSelfTransitions: false,
    validateStateChange: true,
    recordHistory: true
  });
  
  // Create state transition manager with default states and transitions
  const stateManager = new StateTransitionManager(stateMachine, {
    autoSetupTransitions: true,
    defaultStates: ['INIT', 'READY', 'RUNNING', 'ERROR']
  });
  
  const router = new Router();

  // Load configuration
  let configResult;
  try {
    configResult = await configManager.loadConfig(configPath);
    if (!configResult.success) {
      console.error('Failed to load configuration:', configResult.error);
      if (configResult.errors) {
        console.error('Errors:', configResult.errors);
      }
      // Continue with default configuration
      console.warn('Using default configuration');
    } else {
      console.log('Configuration loaded successfully');
    }
  } catch (error) {
    console.error('Error loading configuration:', error.message);
    // Continue with default configuration
    console.warn('Using default configuration');
  }
  
  // Add routes for server management
  router.addRoute('/status', {
    GET: async () => ({
      status: 'success',
      state: stateMachine.getCurrentState()?.name,
      config: configManager.getConfig()
    })
  });
  
  router.addRoute('/states', {
    GET: async () => ({
      status: 'success',
      states: stateMachine.getStateNames(),
      current: stateMachine.getCurrentState()?.name
    })
  });
  
  router.addRoute('/transitions', {
    GET: async () => ({
      status: 'success',
      transitions: stateManager.getPossibleTransitions()
    })
  });

  // Create HTTP server
  const server = http.createServer(async (req, res) => {
    try {
      // Parse request URL
      const url = new URL(req.url, `http://${req.headers.host}`);
      
      // Set CORS headers
      res.setHeader('Access-Control-Allow-Origin', '*');
      res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
      res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
      
      if (req.method === 'OPTIONS') {
        res.writeHead(204);
        res.end();
        return;
      }

      // Parse request body for POST requests
      let body = {};
      if (req.method === 'POST') {
        body = await parseBody(req);
      }

      // Handle request through router
      try {
        const result = await router.handleRequest(
          url.pathname,
          req.method,
          body
        );

        // Send success response
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify(result));
      } catch (routeError) {
        // Determine status code based on error
        const status = routeError.message.includes('not found') ? 404 : 
                    routeError.message.includes('not allowed') ? 405 : 
                    500;

        // Send error response
        res.writeHead(status, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
          success: false,
          error: routeError.message
        }));
      }
    } catch (error) {
      // Server error handling
      console.error('Server error:', error);
      res.writeHead(500, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({
        success: false,
        error: 'Internal server error'
      }));
      
      // Transition to error state
      await stateManager.safeTransitionTo('ERROR');
    }
  });

  // Start server with proper state transitions
  const startServer = async () => {
    const config = configManager.getConfig();
    let port = 8082; // Default container port
    
    // Extract port from config
    if (config.port) {
      if (typeof config.port === 'object' && config.port.container) {
        port = config.port.container;
      } else if (typeof config.port === 'number') {
        port = config.port;
      }
    }
    
    return new Promise((resolve, reject) => {
      try {
        // First transition to READY state
        stateManager.safeTransitionTo('READY')
          .then(() => {
            server.listen(port, async () => {
              // Then transition to RUNNING state
              await stateManager.safeTransitionTo('RUNNING');
              console.log(`Server running at http://localhost:${port}`);
              
              // Show host port mapping if available
              if (config.port && config.port.host) {
                console.log(`(mapped from ${config.port.host})`);
              }
              resolve(server);
            });
          })
          .catch(reject);
      } catch (error) {
        reject(error);
      }
    });
  };

  // Return server interface
  return {
    start: startServer,
    stop: () => {
      return new Promise((resolve) => {
        stateManager.safeTransitionTo('INIT')
          .then(() => {
            server.close(() => {
              console.log('Server stopped');
              resolve();
            });
          })
          .catch(() => {
            server.close(() => {
              console.log('Server stopped with errors');
              resolve();
            });
          });
      });
    },
    getState: () => stateMachine.getCurrentState(),
    getConfig: () => configManager.getConfig(),
    getStateManager: () => stateManager
  };
};

// Request body parser
const parseBody = (req) => {
  return new Promise((resolve, reject) => {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
      try {
        resolve(body ? JSON.parse(body) : {});
      } catch (error) {
        reject(new Error('Invalid JSON'));
      }
    });
    req.on('error', reject);
  });
};

// Start application
async function main() {
  try {
    const configPath = process.argv[2] || '.polycallrc';
    console.log(`Loading configuration from: ${configPath}`);
    
    const server = await createServer(configPath);
    await server.start();
    
    // Handle graceful shutdown
    process.on('SIGINT', async () => {
      console.log('\nShutting down server...');
      await server.stop();
      process.exit(0);
    });
    
    process.on('SIGTERM', async () => {
      console.log('\nShutting down server...');
      await server.stop();
      process.exit(0);
    });
  } catch (error) {
    console.error('Failed to start server:', error);
    process.exit(1);
  }
}

// Run if this is the main module
if (require.main === module) {
  main();
}


module.exports = {
  createServer,
  parseBody
};