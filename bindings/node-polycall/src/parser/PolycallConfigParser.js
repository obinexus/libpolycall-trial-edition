// src/parser/PolycallConfigParser.js
const fs = require('fs').promises;
const path = require('path');
const { createTokenizer } = require('./PolycallConfigTokenizer');
const { validatePolycallConfig } = require('./PolycallConfigValidator');
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

/**
 * Pure function to create tokens with Point-Free style
 * @param {string} type - Token type
 * @param {string} value - Token value
 * @param {number} line - Line number
 * @param {number} column - Column number
 * @returns {Object} Token object
 */
const createToken = curry((type, value, line, column) => ({
    type,
    value,
    line,
    column,
    toString: () => `${type}(${value})`
}));

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
 * Scan whitespace tokens in input
 * @param {string} input - Input text
 * @param {number} position - Current position
 * @param {number} line - Current line
 * @param {number} column - Current column
 * @returns {Object} Scanning result
 */
const scanWhitespace = (input, position, line, column) => {
    let length = 0;
    while (position + length < input.length && /[ \t]/.test(input[position + length])) {
        length++;
    }
    return {
        token: null, // No token for whitespace
        advance: length,
        newColumn: column + length
    };
};

/**
 * Scan comment tokens in input
 * @param {string} input - Input text
 * @param {number} position - Current position
 * @param {number} line - Current line
 * @param {number} column - Current column
 * @returns {Object} Scanning result
 */
const scanComment = (input, position, line, column) => {
    let value = '';
    let length = 0;
    
    // Ensure we're starting with a # character
    if (input[position] !== '#') {
        return {
            token: null,
            advance: 0,
            newColumn: column
        };
    }
    
    while (position + length < input.length && input[position + length] !== '\n') {
        value += input[position + length];
        length++;
    }
    
    return {
        token: createToken(TokenType.COMMENT, value, line, column),
        advance: length,
        newColumn: column + length
    };
};

/**
 * Scan key tokens in input
 * @param {string} input - Input text
 * @param {number} position - Current position
 * @param {number} line - Current line
 * @param {number} column - Current column
 * @returns {Object} Scanning result
 */
const scanKey = (input, position, line, column) => {
    let value = '';
    let length = 0;
    
    const isDirective = ['server', 'network'].some(dir => 
        input.substring(position).startsWith(dir + ' '));
    
    if (isDirective) {
        // Handle directives like "server node 3000:8080"
        const directiveEnd = input.indexOf(' ', position);
        if (directiveEnd === -1) {
            // Invalid directive format
            return {
                token: null,
                advance: 0,
                newColumn: column
            };
        }
        
        const directive = input.substring(position, directiveEnd);
        
        return {
            token: createToken(TokenType.DIRECTIVE, directive, line, column),
            advance: directive.length,
            newColumn: column + directive.length
        };
    }
    
    // Regular key
    while (position + length < input.length && 
           !/[\s=]/.test(input[position + length])) {
        value += input[position + length];
        length++;
    }
    
    return {
        token: createToken(TokenType.KEY, value, line, column),
        advance: length,
        newColumn: column + length
    };
};

/**
 * Scan value tokens in input
 * @param {string} input - Input text
 * @param {number} position - Current position
 * @param {number} line - Current line
 * @param {number} column - Current column
 * @returns {Object} Scanning result
 */
const scanValue = (input, position, line, column) => {
    let value = '';
    let length = 0;
    
    // Skip initial whitespace
    while (position + length < input.length && /[ \t]/.test(input[position + length])) {
        length++;
    }
    
    const startColumn = column + length;
    
    // Read value until newline or comment
    while (position + length < input.length && 
           input[position + length] !== '\n' && 
           input[position + length] !== '#') {
        value += input[position + length];
        length++;
    }
    
    return {
        token: createToken(TokenType.VALUE, value.trim(), line, startColumn),
        advance: length,
        newColumn: column + length
    };
};

/**
 * Scan parameter tokens for directives
 * @param {string} input - Input text
 * @param {number} position - Current position
 * @param {number} line - Current line
 * @param {number} column - Current column
 * @returns {Object} Scanning result
 */
const scanParameter = (input, position, line, column) => {
    let value = '';
    let length = 0;
    
    // Skip initial whitespace
    while (position + length < input.length && /[ \t]/.test(input[position + length])) {
        length++;
    }
    
    const startColumn = column + length;
    
    // Read parameter value until whitespace, newline or comment
    while (position + length < input.length && 
           !/[ \t\n#]/.test(input[position + length])) {
        value += input[position + length];
        length++;
    }
    
    return {
        token: createToken(TokenType.PARAMETER, value, line, startColumn),
        advance: length,
        newColumn: column + length
    };
};

/**
 * Tokenize input string into token stream
 * @param {string} input - Input text
 * @returns {Array} Array of tokens
 */
const tokenizeInput = (input) => {
    const tokens = [];
    let position = 0;
    let line = 1;
    let column = 1;
    
    while (position < input.length) {
        const char = input[position];
        let result = null;
        
        if (char === ' ' || char === '\t') {
            // Whitespace
            result = scanWhitespace(input, position, line, column);
        } else if (char === '#') {
            // Comment
            result = scanComment(input, position, line, column);
        } else if (char === '=') {
            // Equals
            result = {
                token: createToken(TokenType.EQUALS, '=', line, column),
                advance: 1,
                newColumn: column + 1
            };
        } else if (char === '\n') {
            // Newline
            result = {
                token: createToken(TokenType.NEWLINE, '\\n', line, column),
                advance: 1,
                newColumn: 1,
                newLine: line + 1
            };
        } else {
            // Handle key, directive, or parameter
            const prevToken = tokens[tokens.length - 1];
            
            if (prevToken && prevToken.type === TokenType.DIRECTIVE) {
                // After a directive, scan parameters
                result = scanParameter(input, position, line, column);
            } else if (prevToken && prevToken.type === TokenType.EQUALS) {
                // After equals, scan value
                result = scanValue(input, position, line, column);
            } else {
                // Otherwise scan for key or directive
                result = scanKey(input, position, line, column);
            }
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
    tokens.push(createToken(TokenType.EOF, '', line, column));
    return tokens;
};

/**
 * Parse a single token into a node
 * @param {Object} token - Token to parse
 * @returns {Object|null} Node or null
 */
const parseToken = (token) => {
    switch (token.type) {
        case TokenType.KEY:
            return createNode(NodeType.KEY_VALUE, token.value, null);
        case TokenType.DIRECTIVE:
            if (token.value === 'server') {
                return createNode(NodeType.SERVER, 'server', []);
            } else if (token.value === 'network') {
                return createNode(NodeType.NETWORK, 'network', null);
            }
            return null;
        case TokenType.COMMENT:
            return createNode(NodeType.COMMENT, null, token.value);
        default:
            return null;
    }
};

/**
 * Build AST from parsed tokens
 * @param {Object} ast - Current AST
 * @param {Object} node - Current node
 * @returns {Object} Updated AST
 */
const buildAst = (ast, node) => {
    if (!node) return ast;
    
    // Handle different node types
    if (node.type === NodeType.SERVER) {
        ast.children.push(node);
    } else if (node.type === NodeType.KEY_VALUE) {
        ast.children.push(node);
    } else if (node.type === NodeType.COMMENT) {
        ast.children.push(node);
    }
    
    return ast;
};

/**
 * Token stream implementation with Point-Free style
 * @param {Array} tokens - Array of tokens
 * @returns {Object} Token stream object
 */
const createTokenStream = (tokens) => {
    let position = 0;
    
    return {
        next: () => position < tokens.length ? tokens[position++] : null,
        peek: () => position < tokens.length ? tokens[position] : null,
        hasMore: () => position < tokens.length,
        reset: () => { position = 0; },
        tokens: () => [...tokens],
        position: () => position
    };
};

/**
 * Parser implementation
 * @param {Array} transforms - Array of transform functions
 * @returns {Object} Parser object
 */
const createParser = (transforms = []) => {
    // Transformations to apply to parsed nodes
    const defaultTransforms = [
        // Port transform
        (node) => {
            if (node.type === NodeType.KEY_VALUE && node.key === 'port' && typeof node.value === 'string') {
                const parts = node.value.split(':');
                if (parts.length === 2) {
                    return {
                        ...node,
                        value: {
                            host: parseInt(parts[0], 10),
                            container: parseInt(parts[1], 10)
                        }
                    };
                }
            }
            return node;
        },
        
        // Server directive transform
        (node) => {
            if (node.type === NodeType.SERVER && Array.isArray(node.value) && node.value.length >= 2) {
                const [serverType, portMapping] = node.value;
                const parts = portMapping.split(':');
                return {
                    ...node,
                    value: {
                        type: serverType,
                        host: parseInt(parts[0], 10),
                        container: parseInt(parts[1], 10)
                    }
                };
            }
            return node;
        },
        
        // Network directive transform
        (node) => {
            if (node.type === NodeType.NETWORK && node.value === 'start') {
                return {
                    ...node,
                    value: { 
                        enabled: true 
                    }
                };
            }
            return node;
        },
        
        // Apply custom transforms
        ...transforms
    ];
    
    /**
     * Process a stream of tokens into an AST
     * @param {Object} stream - Token stream
     * @returns {Object} AST root node
     */
    const processTokens = (stream) => {
        const root = createNode(NodeType.ROOT, 'root', null, []);
        let currentNode = null;
        
        while (stream.hasMore()) {
            const token = stream.next();
            
            switch (token.type) {
                case TokenType.DIRECTIVE:
                    if (token.value === 'server') {
                        // Process server directive with parameters
                        currentNode = createNode(NodeType.SERVER, 'server', []);
                        root.children.push(currentNode);
                        
                        // Collect parameters
                        while (stream.hasMore() && stream.peek().type === TokenType.PARAMETER) {
                            currentNode.value.push(stream.next().value);
                        }
                    } else if (token.value === 'network') {
                        // Process network directive
                        const nextToken = stream.peek();
                        if (nextToken && nextToken.type === TokenType.PARAMETER && nextToken.value === 'start') {
                            stream.next(); // Consume 'start' parameter
                            currentNode = createNode(NodeType.NETWORK, 'network', 'start');
                        } else {
                            currentNode = createNode(NodeType.NETWORK, 'network', null);
                        }
                        root.children.push(currentNode);
                    }
                    break;
                    
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
        
        // Apply transforms to each node
        root.children = root.children.map(node => 
            defaultTransforms.reduce((n, transform) => transform(n), node)
        );
        
        return root;
    };
    
    return {
        parse: (input) => {
            const tokens = tokenizeInput(input);
            const stream = createTokenStream(tokens);
            return processTokens(stream);
        },
        parseFile: async (filePath) => {
            try {
                const content = await fs.readFile(filePath, 'utf8');
                return processTokens(createTokenStream(tokenizeInput(content)));
            } catch (error) {
                throw new Error(`Failed to parse file: ${error.message}`);
            }
        }
    };
};

/**
 * Convert config AST to configuration object
 * @param {Object} ast - AST root node
 * @returns {Object} Configuration object
 */
const astToConfig = (ast) => {
    if (!ast || !ast.children) return {};
    
    const config = {};
    const servers = [];
    
    ast.children.forEach(node => {
        if (node.type === NodeType.KEY_VALUE) {
            config[node.key] = node.value;
        } else if (node.type === NodeType.SERVER) {
            servers.push(node.value);
        } else if (node.type === NodeType.NETWORK && node.value === 'start') {
            config.network_enabled = true;
        }
    });
    
    if (servers.length > 0) {
        config.servers = servers;
    }
    
    return config;
};

/**
 * Convert config object to string representation
 * @param {Object} config - Configuration object
 * @returns {string} String representation of config
 */
const configToString = (config) => {
    if (!config) return '';
    
    const lines = [];
    
    // Add server definitions
    if (config.servers) {
        config.servers.forEach(server => {
            lines.push(`server ${server.type} ${server.host}:${server.container}`);
        });
        lines.push('');
    }
    
    // Add network configuration
    if (config.network_enabled) {
        lines.push('network start');
    }
    
    // Add other key-value pairs
    Object.entries(config).forEach(([key, value]) => {
        if (key !== 'servers' && key !== 'network_enabled') {
            if (typeof value === 'object' && value !== null) {
                if ('host' in value && 'container' in value) {
                    lines.push(`${key}=${value.host}:${value.container}`);
                } else {
                    lines.push(`${key}=${JSON.stringify(value)}`);
                }
            } else {
                lines.push(`${key}=${value}`);
            }
        }
    });
    
    return lines.join('\n');
};

/**
 * Main function to parse Polycall configuration
 * @param {string} content - Configuration content
 * @returns {Object} Parsed configuration result
 */
const parsePolycallConfig = (content) => {
    const parser = createParser();
    const ast = parser.parse(content);
    const config = astToConfig(ast);
    
    return {
        ast,
        config,
        validation: validatePolycallConfig(config),
        toJSON: () => JSON.stringify(config, null, 2),
        toString: () => configToString(config)
    };
};

module.exports = {
    TokenType,
    NodeType,
    createToken,
    createNode,
    tokenizeInput,
    createTokenStream,
    createParser,
    parseToken,
    parsePolycallConfig,
    astToConfig,
    configToString
};