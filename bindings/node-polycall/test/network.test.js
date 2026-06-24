// test/network.test.js
const { expect } = require('chai');
const { createServer, createEndpoint, processMessage } = require('../src/network');
const { pipe, curry } = require('../src/utils');

describe('Network Layer', () => {
  describe('Endpoint Creation', () => {
    const defaultConfig = {
      port: { host: 3000, container: 8080 },
      maxConnections: 100
    };

    it('should create an endpoint with valid configuration', () => {
      const createConfiguredEndpoint = pipe(
        createEndpoint,
        curry(configureEndpoint)(defaultConfig)
      );

      const endpoint = createConfiguredEndpoint({ type: 'server' });
      
      expect(endpoint).to.deep.include({
        type: 'server',
        port: defaultConfig.port,
        maxConnections: defaultConfig.maxConnections
      });
    });
  });

  describe('Message Processing', () => {
    // Test data
    const validMessage = {
      type: 'command',
      payload: Buffer.from('test'),
      metadata: { sequence: 1 }
    };

    // Create processing pipeline using point-free style
    const processCommandMessage = pipe(
      validateMessage,
      transformMessage,
      executeCommand
    );

    it('should process valid messages', async () => {
      const result = await processCommandMessage(validMessage);
      expect(result.success).to.be.true;
    });

    it('should handle invalid messages', async () => {
      const invalidMessage = { type: 'unknown' };
      const result = await processCommandMessage(invalidMessage);
      expect(result.success).to.be.false;
      expect(result.error).to.exist;
    });
  });

  describe('Server Operations', () => {
    const testPort = { host: 3001, container: 8081 };
    
    it('should create and start server', async () => {
      const server = await createServer({
        port: testPort,
        handlers: {
          onConnection: () => {},
          onMessage: () => {},
          onError: () => {}
        }
      });

      expect(server.isRunning()).to.be.true;
      await server.stop();
    });

    it('should handle client connections', async () => {
      const connections = [];
      const server = await createServer({
        port: testPort,
        handlers: {
          onConnection: (client) => connections.push(client),
          onError: () => {}
        }
      });

      // Simulate client connection
      await connectTestClient(testPort);
      expect(connections).to.have.lengthOf(1);
      await server.stop();
    });
  });
});

// Implementation file: src/network.js

// Pure function to create endpoint configuration
const createEndpoint = curry((type, options = {}) => ({
  type,
  port: options.port || { host: 8080, container: 8080 },
  maxConnections: options.maxConnections || 10,
  handlers: options.handlers || {}
}));

// Configure endpoint with specific settings
const configureEndpoint = curry((config, endpoint) => ({
  ...endpoint,
  port: config.port,
  maxConnections: config.maxConnections
}));

// Message validation using point-free style
const validateMessage = curry((message) => {
  const validators = {
    command: msg => msg.type === 'command' && msg.payload,
    event: msg => msg.type === 'event' && msg.name
  };

  const validator = validators[message.type];
  if (!validator || !validator(message)) {
    return { 
      success: false, 
      error: 'Invalid message format',
      message 
    };
  }

  return { success: true, message };
});

// Message transformation
const transformMessage = curry((result) => {
  if (!result.success) return result;

  const { message } = result;
  return {
    success: true,
    message: {
      ...message,
      timestamp: Date.now(),
      transformed: true
    }
  };
});

// Command execution
const executeCommand = curry(async (result) => {
  if (!result.success) return result;

  try {
    // Execute command logic here
    return {
      success: true,
      result: 'Command executed'
    };
  } catch (error) {
    return {
      success: false,
      error: error.message
    };
  }
});

// Server creation with functional approach
const createServer = curry(async (config) => {
  const state = {
    running: false,
    connections: new Set(),
    handlers: config.handlers || {}
  };

  // Pure functions for server operations
  const start = async () => {
    state.running = true;
    return state;
  };

  const stop = async () => {
    state.running = false;
    state.connections.clear();
    return state;
  };

  const handleConnection = curry((connection) => {
    state.connections.add(connection);
    if (state.handlers.onConnection) {
      state.handlers.onConnection(connection);
    }
  });

  const handleMessage = curry((connection, message) => {
    if (state.handlers.onMessage) {
      state.handlers.onMessage(connection, message);
    }
  });

  // Initialize server
  await start();

  // Return server interface
  return {
    isRunning: () => state.running,
    getConnections: () => Array.from(state.connections),
    stop,
    handleConnection,
    handleMessage
  };
});

module.exports = {
  createEndpoint,
  configureEndpoint,
  createServer,
  validateMessage,
  transformMessage,
  executeCommand
};