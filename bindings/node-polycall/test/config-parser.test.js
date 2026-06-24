const { parseConfig, transformConfig, validateConfig,pipe } = require('../src/index.js');
describe('Configuration Parser', () => {
  // Test data
  const validConfig = `
    port=3000:8080
    server_type=node
    log_level=info
    max_connections=100
  `;

  const defaultConfig = {
    port: { host: 8080, container: 8080 },
    server_type: 'node',
    log_level: 'info',
    max_connections: 100
  };

  describe('parseConfig', () => {
    it('should parse valid configuration string', () => {
      const result = parseConfig(validConfig);
      expect(result).to.deep.include({
        port: '3000:8080',
        server_type: 'node',
        log_level: 'info',
        max_connections: '100'
      });
    });

    it('should handle empty lines and comments', () => {
      const configWithComments = `
        # Server configuration
        port=3000:8080
        
        # Server type
        server_type=node`;
      
      const result = parseConfig(configWithComments);
      expect(result).to.deep.include({
        port: '3000:8080',
        server_type: 'node'
      });
    });
  });

  describe('transformConfig', () => {
    it('should transform port string to object', () => {
      const input = { port: '3000:8080' };
      const result = transformConfig(input);
      expect(result.port).to.deep.equal({
        host: 3000,
        container: 8080
      });
    });

    it('should handle numeric port values', () => {
      const input = { port: 8080 };
      const result = transformConfig(input);
      expect(result.port).to.deep.equal({
        host: 8080,
        container: 8080
      });
    });
  });

  describe('validateConfig', () => {
    it('should validate port configuration', () => {
      const validInput = {
        port: { host: 3000, container: 8080 }
      };
      const result = validateConfig(validInput);
      expect(result.isValid).to.be.true;
    });

    it('should validate server type', () => {
      const validInput = { server_type: 'node' };
      const invalidInput = { server_type: 'invalid' };

      expect(validateConfig(validInput).isValid).to.be.true;
      expect(validateConfig(invalidInput).isValid).to.be.false;
    });
  });

  describe('Config Pipeline', () => {
    // Create point-free composition of config operations
    const processConfig = pipe(
      parseConfig,
      transformConfig,
      validateConfig,
      mergeWithDefaults(defaultConfig)
    );

    it('should process valid configuration end-to-end', () => {
      const result = processConfig(validConfig);
      
      expect(result.isValid).to.be.true;
      expect(result.config).to.deep.include({
        port: { host: 3000, container: 8080 },
        server_type: 'node',
        log_level: 'info'
      });
    });

    it('should handle invalid configurations', () => {
      const invalidConfig = `
        port=invalid
        server_type=unknown
      `;

      const result = processConfig(invalidConfig);
      expect(result.isValid).to.be.false;
      expect(result.errors).to.be.an('array').that.is.not.empty;
    });
  });
});

