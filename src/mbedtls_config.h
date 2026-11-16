#ifndef MBEDTLS_CONFIG_H_
#define MBEDTLS_CONFIG_H_

/*
 * mbedTLS, the SDK's, for one use: the SHA-256 of an image before it is tried (ADR-004, update.c). Read through the
 * SDK's pico_mbedtls_config.h. Nothing else of mbedTLS is built in
 */
#define MBEDTLS_SHA256_C

#endif  // MBEDTLS_CONFIG_H_
