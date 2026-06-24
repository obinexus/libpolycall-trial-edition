// src/parser/PolycallConfigTokenizer.js
const { pipe, curry, map } = require('../utils');

// Token type definitions with validation rules
const TOKEN_PATTERNS = {
    KEY: /^[a-zA-Z_][a-zA-Z0-9_]*/,
    EQUALS: /^=/,
    VALUE: /^[^\n#]*/,
    COMMENT: /^#.*/,
    NEWLINE: /^\n/,
    WHITESPACE: /^[ \t]+/
};

// Token creation with proper position tracking
const createToken = curry((type, value, line, column) => ({
    type,
    value: value.trim(),
    line,
    column,
    toString: () => `${type}(${value})`
}));

// Character classification functions
const isWhitespace = char => /[\s\t]/.test(char);
const isComment = char => char === '#';
const isNewline = char => char === '\n';
const isEquals = char => char === '=';

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

// Validate token patterns
const validateToken = curry((pattern, token) => {
    if (!token || !token.value) return false;
    return pattern.test(token.value);
});

// Token pattern validation
const validators = {
    KEY: validateToken(TOKEN_PATTERNS.KEY),
    VALUE: validateToken(TOKEN_PATTERNS.VALUE),
    COMMENT: validateToken(TOKEN_PATTERNS.COMMENT)
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
        validate: () => tokens.every(token => 
            !validators[token.type] || validators[token.type](token)
        )
    };
};

// Error types for detailed reporting
const TokenizerError = {
    INVALID_KEY: 'Invalid key format',
    INVALID_VALUE: 'Invalid value format',
    UNEXPECTED_TOKEN: 'Unexpected token',
    INCOMPLETE_PAIR: 'Incomplete key-value pair'
};

// Create a tokenizer instance with error tracking
const createTokenizer = () => {
    let errors = [];
    let warnings = [];

    const addError = (message, token) => {
        errors.push({
            message,
            line: token.line,
            column: token.column,
            value: token.value
        });
    };

    const addWarning = (message, token) => {
        warnings.push({
            message,
            line: token.line,
            column: token.column,
            value: token.value
        });
    };

    return {
        tokenize: (input) => {
            errors = [];
            warnings = [];
            const stream = createTokenStream(input);

            if (!stream.validate()) {
                const tokens = stream.getTokens();
                tokens.forEach(token => {
                    if (validators[token.type] && !validators[token.type](token)) {
                        addError(TokenizerError.INVALID_KEY, token);
                    }
                });
            }

            return {
                stream,
                errors: () => [...errors],
                warnings: () => [...warnings],
                hasErrors: () => errors.length > 0
            };
        }
    };
};

module.exports = {
    createTokenizer,
    tokenizeInput,
    createTokenStream,
    TokenizerError,
    TOKEN_PATTERNS
};