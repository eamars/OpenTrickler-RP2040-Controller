#ifndef MEASURE_MBEDTLS_CONFIG_H_
#define MEASURE_MBEDTLS_CONFIG_H_

/*
    mbedTLS for ADR-010's measure build (gate 1): a TLS 1.2 client for GitHub's two chains, an ECDSA one (P-256 under
    P-384) and an RSA one (2048 under 4096), and the local OTA server's test chains of the same kinds. Included by
    src/mbedtls_config.h only when ADR010_MEASURE is set. No clock: certificates' dates are not checked
*/

// mbedTLS's allocations go where measure_tls.c says, and are counted there
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY

// Random numbers: the SDK's hardware source (pico_mbedtls.c)
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C

// Ciphers and hashes for TLS 1.2's AES-GCM suites; AES's tables kept in flash
#define MBEDTLS_CIPHER_C
#define MBEDTLS_AES_C
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_AES_FEWER_TABLES
#define MBEDTLS_GCM_C
#define MBEDTLS_MD_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C

// Public keys: ECDSA and ECDHE on P-256 and P-384, RSA up to 4096 bits; small windows, for RAM
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_WINDOW_SIZE 2
#define MBEDTLS_ECP_FIXED_POINT_OPTIM 0
// Fast reduction for the NIST curves, and the multiply in assembly: without them a handshake with GitHub's ECDSA
// chain took 19.8 s on the Pico 2 W at -Os (148 s at -O0)
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21
#define MBEDTLS_MPI_MAX_SIZE 512
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_OID_C

// Certificates, the roots given as PEM
#define MBEDTLS_BASE64_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C

// TLS 1.2, client only. A record from the server can be 16 KB; what the unit sends is small
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_SSL_IN_CONTENT_LEN 16384
#define MBEDTLS_SSL_OUT_CONTENT_LEN 2048

#endif  // MEASURE_MBEDTLS_CONFIG_H_
