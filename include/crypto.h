/* ============================================================
 *  crypto.h  --  SHA-256 + ECDSA wrappers over OpenSSL 3.x
 *
 *  The system maintains a single ECDSA (P-256) admin keypair
 *  used to sign every attendance block.  The keys live in:
 *     keys/admin_priv.pem    (the signing key)
 *     keys/admin_pub.pem     (used to verify chain integrity)
 *  Both are auto-generated on first run.
 * ============================================================ */
#ifndef CRYPTO_H
#define CRYPTO_H

#include <stddef.h>

#define HASH_HEX_LEN   65          /* 64 hex digits + '\0'      */
#define SIG_MAX_LEN    72          /* DER ECDSA P-256 sig max   */
#define KEY_DIR        "keys"
#define KEY_FILE_PRIV  "keys/admin_priv.pem"
#define KEY_FILE_PUB   "keys/admin_pub.pem"

/* Initialise OpenSSL state and load (or generate) the admin keypair.
 * Returns 0 on success, -1 on failure. */
int  crypto_init(void);

/* Release any cached crypto state. */
void crypto_cleanup(void);

/* Compute SHA-256 of `data` and write it to `out_hex` as
 * a 64-char lowercase hex string + NUL.  out_hex must be HASH_HEX_LEN. */
void sha256_hex(const unsigned char *data, size_t len,
                char out_hex[HASH_HEX_LEN]);

/* Produce an ECDSA signature of `data` (length `len`) using the admin
 * private key.  Writes at most `sig_max` bytes into `sig_out`.
 * Returns the actual signature length, or 0 on failure. */
size_t sign_data(const unsigned char *data, size_t len,
                 unsigned char *sig_out, size_t sig_max);

/* Verify an ECDSA signature against the admin public key.
 * Returns 1 on valid, 0 on invalid, -1 on internal error. */
int verify_data(const unsigned char *data, size_t len,
                const unsigned char *sig, size_t sig_len);

/* Return the SHA-256 fingerprint of the admin public key (for display). */
void admin_pubkey_fingerprint(char out_hex[HASH_HEX_LEN]);

#endif /* CRYPTO_H */
