// src/parser/PolycallParser.js
const fs = require('fs').promises;
const path = require('path');
const { pipe, curry, map, reduce, filter } = require('../utils');

/**
 * Polycall Configuration Node Types
 */
const NodeType = {
    ROOT: 'ROOT',
    SECTION: 'SECTION',
    KEY_VALUE: 'KEY_VALUE',
    COMMENT: 'COMMENT',
    SERVER: 'SERVER',
    NETWORK: 'NETWORK'
};

/**
 * Token type definitions
 */
const TokenType = {
    KEY: 'KEY',
    EQUALS: 'EQUALS',
    VALUE: 'VALUE',
    COMMENT: 'COMMENT',
    NEWLINE: 'NEWLINE',
    EOF: 'EOF',
    DIRECTIVE: 'DIRECTIVE',
    PARAMETER: 'PARAMETER'
};

// Import here to avoid circular dependencies
const createTokenizer = () => {
    // Character classification functions
    const isWhitespace = char => /[\s\t]/.test(char);
    const isComment = char => char === '#';
    const isNewline = char => char === '\n';
    const isEquals = char => char === '=';
    
    // Token creation with proper position tracking
    const createToken = curry((type, value, line, column) => ({
        type,
        value: value.trim(),
        line,
        column,
        toString: () => `${type}(${value})`
    }));
    
    // Pure tokenization functions
    const scanWhitespace = curry((input, position, line, column) => {
        let length = 0;
        while (position + length < input.length && isWhitespace(input[position + length])) {
            length++;
        }
        return {
            token: null,
            advance: length,
            newColumn: column + length
        };
    });
    
    const scanComment = curry((input, position, line, column) => {
        let value = '';
        let length = 0;
        
        while (position + length < input.length && !isNewline(input[position + length])) {
            value += input[position + length];
            length++;
        }
        
        return {
            token: createToken('COMMENT', value, line, column),
            advance: length,
            newColumn: column + length
        };
    });
    
    const scanKey = curry((input, position, line, column) => {
        let value = '';
        let length = 0;
        
        while (position + length < input.length && 
               !isWhitespace(input[position + length]) && 
               !isEquals(input[position + length])) {
            value += input[position + length];
            length++;
        }
        
        return {
            token: createToken('KEY', value, line, column),
            advance: length,
            newColumn: column + length
        };
    });
    
    const scanValue = curry((input, position, line, column) => {
        let value = '';
        let length = 0;
        
        while (position + length < input.length && 
               !isNewline(input[position + length]) && 
               !isComment(input[position + length])) {
            value += input[position + length];
            length++;
        }
        
        return {
            token: createToken('VALUE', value, line, column),
            advance: length,
            newColumn: column + length
        };
    });
    
    // Token stream generator
    const tokenizeInput = (input) => {
        const tokens = [];
        let position = 0;
        let line = 1;
        let column = 1;
        
        while (position < input.length) {
            const char = input[position];
            let result = null;
            
            if (isWhitespace(char)) {
                result = scanWhitespace(input, position, line, column);
            } else if (isComment(char)) {
                result = scanComment(input, position, line, column);
            } else if (isEquals(char)) {
                result = {
                    token: createToken('EQUALS', '=', line, column),
                    advance: 1,
                    newColumn: column + 1
                };
            } else if (isNewline(char)) {
                result = {
                    token: createToken('NEWLINE', '\\n', line, column),
                    advance: 1,
                    newColumn: 1,
                    newLine: line + 1
                };
            } else {
                // Handle key or value based on context
                result = tokens.length > 0 && 
                        tokens[tokens.length - 1].type === 'EQUALS' ?
                    scanValue(input, position, line, column) :
                    scanKey(input, position, line, column);
            }
            
            position += result.advance;
            column = result.newColumn;
            
            if (result.newLine) {
                line = result.newLine;
            }
            
            if (result.token) {
                tokens.push(result.token);
            }
        }
        
        // Add EOF token
        tokens.push(createToken('EOF', '', line, column));
        return tokens;
    };

    // Token stream with validation
    const createTokenStream = (input) => {
        let tokens = tokenizeInput(input);
        let position = 0;
        
        return {
            next: () => position < tokens.length ? tokens[position++] : null,
            peek: () => position < tokens.length ? tokens[position] : null,
            hasMore: () => position < tokens.length,
            reset: () => { position = 0; },
            getTokens: () => [...tokens],
            position: () => position,
            validate: () => true // Simplified validation for this implementation
        };
    };

    let errors = [];
    let warnings = [];

    return {
        tokenize: (input) => {
            errors = [];
            warnings = [];
            const stream = createTokenStream(input);

            return {
                stream,
                errors: () => [...errors],
                warnings: () => [...warnings],
                hasErrors: () => errors.length > 0
            };
        }
    };
};

/**
 * Pure function to create nodes with Point-Free style
 * @param {string} type - Node type
 * @param {string} key - Node key
 * @param {any} value - Node value
 * @param {Array} children - Node children
 * @returns {Object} Node object
 */
const createNode = curry((type, key, value, children = []) => ({
    type,
    key,
    value,
    children,
    metadata: new Map(),
    validate: () => validateNode(type, key, value)
}));

/**
 * Validate a node based on its type and value
 * @param {string} type - Node type
 * @param {string} key - Node key
 * @param {any} value - Node value
 * @returns {boolean} Whether the node is valid
 */
const validateNode = (type, key, value) => {
    switch (type) {
        case NodeType.KEY_VALUE:
            return key !== null && key !== undefined;
        case NodeType.SERVER:
            return key === 'server' && Array.isArray(value) && value.length >= 2;
        case NodeType.NETWORK:
            return key === 'network';
        default:
            return true;
    }
};

/**
 * Simple validation of configuration values
 * @param {Object} config - Configuration object to validate
 * @returns {Object} Validation result with errors and warnings
 */
const validatePolycallConfig = (config) => {
    const errors = [];
    const warnings = [];
    
    // Required fields check
    const requiredFields = ['port', 'server_type'];
    requiredFields.forEach(field => {
        if (!(field in config)) {
            errors.push(`Missing required field: ${field}`);
        }
    });
    
    // Port validation
    if (config.port) {
        if (typeof config.port === 'object') {
            const { host, container } = config.port;
            if (!host || !container || host < 1 || host > 65535 || container < 1 || container > 65535) {
                errors.push('Port must be in format host:container (1-65535)');
            }
        } else if (typeof config.port === 'string') {
            if (!/^\d+:\d+$/.test(config.port)) {
                errors.push('Port must be in format host:container (1-65535)');
            }
        }
    }
    
    // Server type validation
    if (config.server_type && !['node', 'python', 'java', 'go'].includes(config.server_type)) {
        errors.push('Server type must be one of: node, python, java, go');
    }
    
    return {
        isValid: errors.length === 0,
        errors,
        warnings
    };
};

/**
 * Parse a token stream into an AST
 * @param {Object} stream - Token stream to parse
 * @returns {Object} AST root node
 */
const parseTokenStream = (stream) => {
    const root = createNode(NodeType.ROOT, 'root', null, []);
    let currentNode = null;
    
    while (stream.hasMore()) {
        const token = stream.next();
        
        switch (token.type) {
            case TokenType.KEY:
                currentNode = createNode(NodeType.KEY_VALUE, token.value, null);
                root.children.push(currentNode);
                break;
                
            case TokenType.EQUALS:
                // Skip equals token
                break;
                
            case TokenType.VALUE:
                if (currentNode && currentNode.type === NodeType.KEY_VALUE) {
                    currentNode.value = token.value;
                }
                break;
                
            case TokenType.COMMENT:
                root.children.push(createNode(NodeType.COMMENT, null, token.value));
                break;
                
            case TokenType.NEWLINE:
            case TokenType.EOF:
                // Reset current node after newline or EOF
                currentNode = null;
                break;
        }
    }
    
    return root;
};

/**
 * Transform AST into a configuration object
 * @param {Object} ast - AST to transform
 * @returns {Object} Configuration object
 */
const astToConfig = (ast) => {
    if (!ast || !ast.children) return {};
    
    const config = {};
    
    ast.children.forEach(node => {
        if (node.type === NodeType.KEY_VALUE) {
            // Parse special values
            if (node.key === 'port' && node.value && node.value.includes(':')) {
                const [host, container] = node.value.split(':').map(v => parseInt(v, 10));
                config[node.key] = { host, container };
            } else {
                config[node.key] = node.value;
            }
        }
    });
    
    return config;
};

/**
 * Parse configuration content
 * @param {string} content - Configuration file content
 * @returns {Object} Parsed configuration
 */
const parseConfig = (content) => {
    const tokenizer = createTokenizer();
    const result = tokenizer.tokenize(content);
    const ast = parseTokenStream(result.stream);
    const config = astToConfig(ast);
    
    return {
        ast,
        config,
        validation: validatePolycallConfig(config)
    };
};

/**
 * Parse a Polycall RC file
 * @param {string} content - File content to parse
 * @returns {Object} Parsed configuration result
 */
const parsePolycallRC = (content) => {
    const parsedConfig = parseConfig(content);
    
    return {
        root: parsedConfig.ast,
        config: parsedConfig.config,
        validation: parsedConfig.validation,
        toJSON: () => JSON.stringify(parsedConfig.config, null, 2),
        toString: () => configToString(parsedConfig.config)
    };
};

/**
 * Load configuration from file
 * @param {string} filePath - Path to configuration file
 * @returns {Promise<Object>} Parsed configuration
 */
const loadConfigFile = async (filePath) => {
    try {
        const content = await fs.readFile(filePath, 'utf8');
        return parseConfig(content);
    } catch (error) {
        throw new Error(`Failed to load configuration file: ${error.message}`);
    }
};

/**
 * Convert configuration to string representation
 * @param {Object} config - Configuration object
 * @returns {string} String representation
 */
const configToString = (config) => {
    if (!config) return '';
    
    const lines = [];
    
    Object.entries(config).forEach(([key, value]) => {
        if (typeof value === 'object' && value !== null) {
            if ('host' in value && 'container' in value) {
                lines.push(`${key}=${value.host}:${value.container}`);
            } else {
                lines.push(`${key}=${JSON.stringify(value)}`);
            }
        } else {
            lines.push(`${key}=${value}`);
        }
    });
    
    return lines.join('\n');
};

// Export public API
module.exports = {
    // Core parsing functionality
    parsePolycallRC,
    parseConfig,
    loadConfigFile,
    
    // Helper functions
    createNode,
    validateNode,
    validatePolycallConfig,
    configToString,
    astToConfig,
    
    // Types and constants
    NodeType,
    TokenType
};