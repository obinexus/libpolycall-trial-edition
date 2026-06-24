// src/parser/ConfigManager.js
const fs = require('fs').promises;
const path = require('path');
const { pipe, curry } = require('../utils');

/**
 * Configuration Manager for PolyCall
 * Uses a data-oriented approach with point-free style operations
 */
class ConfigManager {
  constructor(initialConfig = {}) {
    this.config = {
      port: { host: 8080, container: 8082 },
      server_type: 'node',
      log_level: 'info',
      max_connections: 100,
      timeout: 30,
      security: {
        requireAuth: false
      },
      ...initialConfig
    };
    this.filePath = null;
    this.errors = [];
    this.warnings = [];
  }

  /**
   * Get current configuration
   * @returns {Object} Current configuration object
   */
  getConfig() {
    return { ...this.config };
  }

  /**
   * Load configuration from file
   * @param {string} filePath - Path to configuration file
   * @returns {Object} Result with success status and config or error
   */
  async loadConfig(filePath) {
    try {
      this.filePath = filePath;
      const content = await fs.readFile(filePath, 'utf8');
      return this.parseConfig(content);
    } catch (error) {
      this.errors.push({
        message: `Failed to load configuration file: ${error.message}`,
        path: filePath
      });
      return {
        success: false,
        error: `Failed to load configuration file: ${error.message}`
      };
    }
  }

  /**
   * Parse configuration content
   * @param {string} content - Configuration file content
   * @returns {Object} Result with success status and config or error
   */
  parseConfig(content) {
    try {
      // Reset errors and warnings
      this.errors = [];
      this.warnings = [];

      // Parse configuration lines
      const config = pipe(
        str => str.split('\n'),
        lines => lines.filter(line => line.trim() && !line.trim().startsWith('#')),
        lines => lines.map(line => {
          const [key, value] = line.split('=').map(s => s.trim());
          if (!key || value === undefined) return null;
          return { key, value: this.parseValue(value) };
        }),
        entries => entries.filter(Boolean),
        entries => Object.fromEntries(entries.map(({ key, value }) => [key, value]))
      )(content);

      // Validate configuration
      const validation = this.validateConfig(config);
      if (!validation.isValid) {
        this.errors.push(...validation.errors);
        this.warnings.push(...(validation.warnings || []));
        return {
          success: false,
          error: "Validation errors detected",
          errors: validation.errors,
          warnings: validation.warnings
        };
      }

      // Update config
      this.updateConfigFromParsed(config);
      
      return {
        success: true,
        config: this.config
      };
    } catch (error) {
      this.errors.push({
        message: `Failed to parse configuration: ${error.message}`
      });
      return {
        success: false,
        error: `Failed to parse configuration: ${error.message}`
      };
    }
  }

  /**
   * Parse configuration value with type conversion
   * @param {string} value - Raw configuration value
   * @returns {any} Parsed value with appropriate type
   */
  parseValue(value) {
    if (!value) return value;
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
    
    // Memory values (GB/MB)
    if (/^\d+[GM]B?$/.test(value)) {
      const match = value.match(/^(\d+)([GM]B?)/);
      const [, size, unit] = match;
      return {
        size: parseInt(size, 10),
        unit: unit.charAt(0)
      };
    }
    
    // Default to string
    return value;
  }

  /**
   * Validate configuration object
   * @param {Object} config - Configuration to validate
   * @returns {Object} Validation result
   */
  validateConfig(config) {
    const errors = [];
    const warnings = [];
    
    // Required fields
    const requiredFields = ['port', 'server_type'];
    requiredFields.forEach(field => {
      if (!(field in config)) {
        errors.push(`Missing required field: ${field}`);
      }
    });
    
    // Validate port
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
    
    // Validate server type
    if (config.server_type && !['node', 'python', 'java', 'go'].includes(config.server_type)) {
      errors.push('Server type must be one of: node, python, java, go');
    }
    
    return {
      isValid: errors.length === 0,
      errors,
      warnings
    };
  }

  /**
   * Update configuration from parsed object
   * @param {Object} parsedConfig - Parsed configuration object
   */
  updateConfigFromParsed(parsedConfig) {
    // Use point-free style to update config
    const updateConfig = curry((config, key, value) => {
      config[key] = value;
      return config;
    });
    
    // Apply each parsed config value to the current config
    Object.entries(parsedConfig).forEach(([key, value]) => {
      updateConfig(this.config)(key, value);
    });
  }

  /**
   * Update configuration from file
   * @param {string} filePath - Path to configuration file
   * @returns {Promise<Object>} Result with success status
   */
  async updateConfig(filePath) {
    return this.loadConfig(filePath);
  }

  /**
   * Get validation errors
   * @returns {Array} List of validation errors
   */
  getErrors() {
    return [...this.errors];
  }

  /**
   * Get validation warnings
   * @returns {Array} List of validation warnings
   */
  getWarnings() {
    return [...this.warnings];
  }

  /**
   * Convert config to string representation
   * @returns {string} String representation of configuration
   */
  toString() {
    const lines = [];
    
    Object.entries(this.config).forEach(([key, value]) => {
      // Handle special cases
      if (typeof value === 'object' && value !== null) {
        if ('host' in value && 'container' in value) {
          lines.push(`${key}=${value.host}:${value.container}`);
        } else if ('size' in value && 'unit' in value) {
          lines.push(`${key}=${value.size}${value.unit}B`);
        } else if (key === 'security') {
          // Flatten security object
          Object.entries(value).forEach(([secKey, secValue]) => {
            lines.push(`security_${secKey}=${secValue}`);
          });
        } else {
          // Flatten other objects
          Object.entries(value).forEach(([subKey, subValue]) => {
            lines.push(`${key}_${subKey}=${subValue}`);
          });
        }
      } else {
        lines.push(`${key}=${value}`);
      }
    });
    
    return lines.join('\n');
  }
}

module.exports = ConfigManager;