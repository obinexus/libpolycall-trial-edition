// src/parser/PolycallConfigValidator.js
const { pipe, curry } = require('../utils');

// Validation rules for specific config values
const CONFIG_RULES = {
    port: {
        pattern: /^\d+:\d+$/,
        validate: value => {
            const [host, container] = value.split(':').map(Number);
            return host > 0 && host < 65536 && container > 0 && container < 65536;
        },
        message: 'Port must be in format host:container (1-65535)'
    },
    server_type: {
        pattern: /^(node|python|java|go)$/,
        validate: value => ['node', 'python', 'java', 'go'].includes(value),
        message: 'Server type must be one of: node, python, java, go'
    },
    log_level: {
        pattern: /^(info|debug|warn|error)$/,
        validate: value => ['info', 'debug', 'warn', 'error'].includes(value),
        message: 'Log level must be one of: info, debug, warn, error'
    },
    max_connections: {
        pattern: /^\d+$/,
        validate: value => Number(value) > 0 && Number(value) <= 10000,
        message: 'Max connections must be between 1 and 10000'
    },
    max_memory: {
        pattern: /^\d+[MG]B?$/,
        validate: value => {
            const match = value.match(/^(\d+)([MG]B?)/);
            if (!match) return false;
            const [, size, unit] = match;
            return unit.startsWith('G') ? Number(size) <= 32 : Number(size) <= 32768;
        },
        message: 'Memory must be specified in MB or GB (max 32GB)'
    },
    timeout: {
        pattern: /^\d+$/,
        validate: value => Number(value) >= 0 && Number(value) <= 300,
        message: 'Timeout must be between 0 and 300 seconds'
    }
};

// Create validation function for a specific rule
const createValidator = curry((rule, value) => {
    if (!rule.pattern.test(value)) {
        return {
            valid: false,
            error: rule.message
        };
    }
    
    if (!rule.validate(value)) {
        return {
            valid: false,
            error: rule.message
        };
    }
    
    return {
        valid: true,
        value: value
    };
});

// Validate a single config entry
const validateConfigEntry = curry((key, value) => {
    const rule = CONFIG_RULES[key];
    if (!rule) {
        return {
            valid: true,
            value: value
        };
    }
    return createValidator(rule, value);
});

// Process validation results
const processValidationResults = results => {
    const errors = [];
    const warnings = [];
    
    results.forEach(result => {
        if (!result.valid) {
            if (result.required) {
                errors.push(result.error);
            } else {
                warnings.push(result.error);
            }
        }
    });
    
    return {
        isValid: errors.length === 0,
        errors,
        warnings
    };
};

// Main validation function with point-free composition
const validateConfig = pipe(
    Object.entries,
    entries => entries.map(([key, value]) => ({
        key,
        ...validateConfigEntry(key, value)
    })),
    processValidationResults
);

// Schema validation for config structure
const validateConfigSchema = config => {
    const requiredFields = ['port', 'server_type'];
    const errors = [];
    
    requiredFields.forEach(field => {
        if (!(field in config)) {
            errors.push(`Missing required field: ${field}`);
        }
    });
    
    return {
        isValid: errors.length === 0,
        errors
    };
};

// Comprehensive config validation
const validatePolycallConfig = config => {
    // Validate schema first
    const schemaValidation = validateConfigSchema(config);
    if (!schemaValidation.isValid) {
        return schemaValidation;
    }
    
    // Then validate values
    return validateConfig(config);
};

module.exports = {
    validatePolycallConfig,
    validateConfigEntry,
    CONFIG_RULES
};