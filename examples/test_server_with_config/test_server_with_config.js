const { expect } = require('chai');
const fs = require('fs').promises;
const path = require('path');
const http = require('http');
const { createServer, parseBody } = require('./server_with_config');

// Functional utilities for point-free style testing
const pipe = (...fns) => x => fns.reduceRight((v, f) => f(v), x);
const curry = (fn) => {
    const arity = fn.length;
    return function curried(...args) {
        if (args.length >= arity) {
            return fn(...args);
        }
        return function(...moreArgs) {
            return curried(...args, ...moreArgs);
        };
    };
};

// Helper function for making HTTP requests with point-free style
const makeRequest = curry((method, path, data = null, port = 8082) => {
    return new Promise((resolve, reject) => {
        const options = {
            hostname: 'localhost',
            port,
            path,
            method,
            headers: {
                'Content-Type': 'application/json'
            }
        };

        const req = http.request(options, (res) => {
            let responseData = '';
            res.on('data', chunk => responseData += chunk);
            res.on('end', () => {
                try {
                    resolve({
                        status: res.statusCode,
                        data: responseData ? JSON.parse(responseData) : null
                    });
                } catch (error) {
                    reject(error);
                }
            });
        });

        req.on('error', reject);

        if (data) {
            req.write(JSON.stringify(data));
        }
        req.end();
    });
});

// Configuration data with functional transformations
const createTestConfig = pipe(
    config => config.trim().split('\n'),
    lines => lines.filter(line => line.trim() && !line.startsWith('#')),
    lines => lines.reduce((config, line) => {
        const [key, value] = line.split('=').map(s => s.trim());
        return { ...config, [key]: value };
    }, {})
);

// Test configuration content
const testConfigContent = `
# Test Server Configuration
port=3000:8082
server_type=node
log_level=info
max_connections=100
timeout=30
`;

describe('Server With Config TDD Test Suite', () => {
    let server;
    let testConfigPath;

    // Lifecycle management with point-free style
    const setupTestConfig = async () => {
        testConfigPath = path.join(__dirname, 'test.polycallrc');
        await fs.writeFile(testConfigPath, testConfigContent);
        return testConfigPath;
    };

    const cleanupTestConfig = async () => {
        try {
            await fs.unlink(testConfigPath);
        } catch (error) {
            console.error('Config cleanup error:', error);
        }
    };

    // Server initialization with error handling
    const initializeServer = async (configPath) => {
        try {
            return await createServer(configPath);
        } catch (error) {
            console.error('Server initialization failed:', error);
            throw error;
        }
    };

    before(async () => {
        await setupTestConfig();
    });

    after(async () => {
        await cleanupTestConfig();
    });

    beforeEach(async () => {
        server = await initializeServer(testConfigPath);
    });

    afterEach(async () => {
        if (server) {
            await server.stop();
        }
    });

    // Configuration Validation Tests
    describe('Configuration Validation', () => {
        it('should parse configuration correctly', () => {
            const parsedConfig = createTestConfig(testConfigContent);
            
            expect(parsedConfig).to.deep.include({
                port: '3000:8082',
                server_type: 'node',
                log_level: 'info'
            });
        });

        it('should load configuration with correct port mapping', async () => {
            const config = server.getConfig();
            
            expect(config.port).to.deep.equal({ 
                host: 3000, 
                container: 8082 
            });
            expect(config.server_type).to.equal('node');
        });
    });

    // State Management Tests with Functional Approach
    describe('State Transition Management', () => {
        const getStateName = server => server.getState().name;
        const transitionAndVerify = curry(async (fromState, toState, srv) => {
            const stateManager = srv.getStateManager();
            await stateManager.safeTransitionTo(toState);
            expect(getStateName(srv)).to.equal(toState);
        });

        it('should start in INIT state', () => {
            expect(getStateName(server)).to.equal('INIT');
        });

        it('should transition through states correctly', async () => {
            await pipe(
                transitionAndVerify('INIT', 'READY'),
                transitionAndVerify('READY', 'RUNNING')
            )(server);
        });

        it('should provide possible state transitions', async () => {
            await server.start();
            const stateManager = server.getStateManager();
            const transitions = stateManager.getPossibleTransitions();
            
            expect(transitions).to.be.an('array');
            expect(transitions).to.include.members(['ERROR', 'READY']);
        });
    });

    // Server Startup and API Tests
    describe('Server API and Lifecycle', () => {
        const getStatusRequest = makeRequest('GET', '/status');
        const getStatesRequest = makeRequest('GET', '/states');

        beforeEach(async () => {
            await server.start();
        });

        it('should start server and respond to status request', async () => {
            const response = await getStatusRequest();
            
            expect(response.status).to.equal(200);
            expect(response.data).to.have.property('state', 'RUNNING');
            expect(response.data).to.have.property('config');
        });

        it('should return available states', async () => {
            const response = await getStatesRequest();
            
            expect(response.status).to.equal(200);
            expect(response.data.states).to.include.members([
                'INIT', 'READY', 'RUNNING', 'ERROR'
            ]);
        });
    });

    // Error Handling and Edge Cases
    describe('Error Handling and Resilience', () => {
        beforeEach(async () => {
            await server.start();
        });

        it('should handle invalid routes gracefully', async () => {
            const invalidRouteRequest = makeRequest('GET', '/non-existent-route');
            const response = await invalidRouteRequest();
            
            expect(response.status).to.equal(404);
            expect(response.data.success).to.be.false;
        });

        it('should transition to ERROR state on critical failures', async () => {
            const stateManager = server.getStateManager();
            await stateManager.safeTransitionTo('ERROR');
            
            expect(server.getState().name).to.equal('ERROR');
            
            const statusResponse = await makeRequest('GET', '/status')();
            expect(statusResponse.data.state).to.equal('ERROR');
        });
    });

    // Configuration Flexibility Tests
    describe('Configuration Flexibility', () => {
        const createFlexibleConfig = async (configContent) => {
            const flexConfigPath = path.join(__dirname, 'flexible_test.polycallrc');
            await fs.writeFile(flexConfigPath, configContent);
            
            const flexServer = await createServer(flexConfigPath);
            await flexServer.start();
            
            return { server: flexServer, configPath: flexConfigPath };
        };

        afterEach(async () => {
            try {
                await fs.unlink(path.join(__dirname, 'flexible_test.polycallrc'));
            } catch {}
        });

        it('should handle alternative port configurations', async () => {
            const altConfigContent = `
            port=4000:9090
            server_type=python
            `;

            const { server: flexServer } = await createFlexibleConfig(altConfigContent);
            const config = flexServer.getConfig();
            
            expect(config.port).to.deep.equal({ host: 4000, container: 9090 });
            expect(config.server_type).to.equal('python');
        });
    });
});