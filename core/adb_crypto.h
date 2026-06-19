#ifndef ADB_CRYPTO_H
#define ADB_CRYPTO_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Embedded RSA for ADB auth (no OpenSSL dependency) ── */

/* Initialize crypto subsystem. Generates 2048-bit keypair on first call */
/* Stores in ~/.android/adbkey (PEM format) */
/* Returns 0 on success, -1 on failure */
int adb_crypto_init(void);

/* Set custom key path */
void adb_crypto_set_keypath(const char *path);

/* Sign a 20-byte ADB auth token with RSA-SHA1 */
/* Returns signature length in sig, or -1 on failure */
int adb_crypto_sign(const unsigned char *token, int token_len,
                     unsigned char *sig, int *sig_len);

/* Get the public key in ADB format (base64 + comment) */
/* Returns string length, or -1 on failure */
int adb_crypto_get_public_key(char *buf, int buf_sz);

/* Generate a new RSA keypair (2048-bit) */
/* Returns 0 on success */
int adb_crypto_generate_keypair(void);

/* Load existing keypair from path */
int adb_crypto_load_keypair(const char *path);

/* Save keypair to path (PEM private key + ADB public key) */
int adb_crypto_save_keypair(const char *path);

/* Free crypto resources */
void adb_crypto_cleanup(void);

/* Check if crypto is initialized */
int adb_crypto_ready(void);

#ifdef __cplusplus
}
#endif

#endif
