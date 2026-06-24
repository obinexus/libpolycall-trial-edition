// src/parser/index.js
const ConfigManager = require('./ConfigManager');

/**
 * Parse configuration value with type conversion
 * @param {string} value - Raw configuration value
 * @returns {any} Parsed configuration value
 */
const parseValue = (value) => {
    if (!value || typeof value !== 'string') return value;
    
    value = value.trim();
    
    // Boolean values
    if (value.toLowerCase() === 'true') return true;
    if (value.toLowerCase() === 'false') return false;
    
    // Number values
    if (/^\d+$/.test(value)) return parseInt(value, 10);
    if (/^\d+\.\d+$/.test(value)) return parseFloat(value);
    
    // Port mapping (host:container)
    if (/^\d+:\d+$/.test(value)) {
        const [host, container] = value.split(':').map(v => parseInt(v, 10));
        return { host, container };
    }
    
    // Default to string
    return value;
};

/**
 * Merge configuration with default values
 * @param {Object} config - User configuration
 * @param {Object} defaults - Default configuration
 * @returns {Object} Merged configuration
 */
const mergeWithDefaults = (config, defaults = {}) => {
    const baseDefaults = {
        port: { host: 8080, container: 8082 },
        server_type: 'node',
        log_level: 'info',
        max_connections: 100,
        timeout: 30,
        security: {
            requireAuth: false
        }
    };
    
    const mergedDefaults = { ...baseDefaults, ...defaults };
    
    // Create a new object with defaults, overridden by config
    return { ...mergedDefaults, ...config };
};

/**
 * Transform configuration based on schema
 * @param {Object} config - Configuration to transform
 * @param {Object} schema - Transformation schema
 * @returns {Object} Transformed configuration
 */
const transformConfig = (config, schema = {}) => {
    if (!config) return config;
    
    const result = { ...config };
    
    // Apply transformations based on schema
    Object.entries(schema).forEach(([key, transform]) => {
        if (key in result && typeof transform === 'function') {
            result[key] = transform(result[key]);
        }
    });
    
    return result;
};

// Export functions from this module
module.exports = {
    // ConfigManager class
    ConfigManager,
    
    // Utility functions
    parseValue,
    mergeWithDefaults,
    transformConfig
};