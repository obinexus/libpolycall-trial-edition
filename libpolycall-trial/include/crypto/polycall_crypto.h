/**
 * @file polycall_crypto.h
 * @brief Cryptographic utilities for LibPolyCall
 *
 * Provides a comprehensive set of cryptographic primitives and security functions
 * designed for use across the LibPolyCall ecosystem, with a focus on:
 * - Secure random number generation
 * - Checksum and integrity verification
 * - Cryptographic key management
 * - Memory sanitization
 */

#ifndef POLYCALL_CRYPTO_H
#define POLYCALL_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Cryptographic algorithm identifiers */
typedef enum {
    POLYCALL_CRYPTO_ALG_SHA256 = 0x01,
    POLYCALL_CRYPTO_ALG_SHA3_256 = 0x02,
    POLYCALL_CRYPTO_ALG_AES_256_GCM = 0x10,
    POLYCALL_CRYPTO_ALG_CHACHA20_POLY1305 = 0x11
} PolycallCryptoAlgorithm;

/** Cryptographic context for advanced operations */
typedef struct {
    uint8_t* key;           /**< Cryptographic key */
    size_t key_length;      /**< Key length in bytes */
    uint8_t* nonce;         /**< Nonce/Initialization Vector */
    size_t nonce_length;    /**< Nonce length in bytes */
    void* internal_state;   /**< Internal cryptographic state */
} PolycallCryptoContext;

/** Secure random number generation modes */
typedef enum {
    POLYCALL_CRYPTO_RNG_SYSTEM = 0x01,  /**< System cryptographically secure RNG */
    POLYCALL_CRYPTO_RNG_CUSTOM = 0x02   /**< Custom entropy source */
} PolycallCryptoRNGMode;

/** Crypto operation status codes */
typedef enum {
    POLYCALL_CRYPTO_SUCCESS = 0,
    POLYCALL_CRYPTO_ERROR_INIT = -1,
    POLYCALL_CRYPTO_ERROR_MEMORY = -2,
    POLYCALL_CRYPTO_ERROR_ENTROPY = -3,
    POLYCALL_CRYPTO_ERROR_ALGORITHM = -4
} PolycallCryptoStatus;

/**
 * @brief Generate cryptographically secure random bytes
 * 
 * Generates high-quality random bytes using the specified RNG mode
 * 
 * @param buffer Output buffer for random bytes
 * @param size Number of bytes to generate
 * @param mode Random number generation mode
 * @return PolycallCryptoStatus indicating success or failure
 */
PolycallCryptoStatus polycall_crypto_generate_random_bytes(
    uint8_t* buffer, 
    size_t size, 
    PolycallCryptoRNGMode mode
);

/**
 * @brief Calculate cryptographic checksum
 * 
 * Computes a secure, non-cryptographic checksum for data integrity verification
 * 
 * @param data Input data
 * @param size Data size
 * @param algorithm Checksum algorithm
 * @return Computed checksum value
 */
uint32_t polycall_crypto_calculate_checksum(
    const void* data, 
    size_t size, 
    PolycallCryptoAlgorithm algorithm
);

/**
 * @brief Verify data integrity using cryptographic checksum
 * 
 * @param data Input data
 * @param size Data size
 * @param expected_checksum Expected checksum value
 * @param algorithm Checksum algorithm
 * @return true if checksum matches, false otherwise
 */
bool polycall_crypto_verify_checksum(
    const void* data, 
    size_t size, 
    uint32_t expected_checksum, 
    PolycallCryptoAlgorithm algorithm
);

/**
 * @brief Securely zero memory to prevent information leakage
 * 
 * Overwrites memory with multiple passes to ensure data cannot be recovered
 * 
 * @param ptr Memory pointer to sanitize
 * @param size Memory size
 */
void polycall_crypto_secure_zero_memory(
    void* ptr, 
    size_t size
);

/**
 * @brief Generate cryptographic key
 * 
 * Creates a new cryptographic key using the specified algorithm
 * 
 * @param context Crypto context to populate
 * @param algorithm Key generation algorithm
 * @param key_length Desired key length in bytes
 * @return PolycallCryptoStatus indicating success or failure
 */
PolycallCryptoStatus polycall_crypto_generate_key(
    PolycallCryptoContext* context,
    PolycallCryptoAlgorithm algorithm,
    size_t key_length
);

/**
 * @brief Rotate cryptographic keys
 * 
 * Generates a new key and securely replaces the existing key
 * 
 * @param context Crypto context to rotate
 * @return PolycallCryptoStatus indicating success or failure
 */
PolycallCryptoStatus polycall_crypto_rotate_key(
    PolycallCryptoContext* context
);

/**
 * @brief Encrypt data
 * 
 * @param context Crypto context
 * @param input Input data
 * @param input_length Input data length
 * @param output Output buffer
 * @param output_length Output buffer length
 * @return PolycallCryptoStatus indicating success or failure
 */
PolycallCryptoStatus polycall_crypto_encrypt(
    const PolycallCryptoContext* context,
    const uint8_t* input,
    size_t input_length,
    uint8_t* output,
    size_t* output_length
);

/**
 * @brief Decrypt data
 * 
 * @param context Crypto context
 * @param input Input data
 * @param input_length Input data length
 * @param output Output buffer
 * @param output_length Output buffer length
 * @return PolycallCryptoStatus indicating success or failure
 */
PolycallCryptoStatus polycall_crypto_decrypt(
    const PolycallCryptoContext* context,
    const uint8_t* input,
    size_t input_length,
    uint8_t* output,
    size_t* output_length
);

/**
 * @brief Clean up and free cryptographic context
 * 
 * Securely frees all resources associated with a crypto context
 * 
 * @param context Crypto context to clean up
 */
void polycall_crypto_context_cleanup(
    PolycallCryptoContext* context
);

/**
 * @brief Get a string description of a crypto status code
 * 
 * @param status Status code to describe
 * @return Descriptive string for the status
 */
const char* polycall_crypto_status_string(
    PolycallCryptoStatus status
);

#ifdef __cplusplus
}
#endif

#endif /* POLYCALL_CRYPTO_H */