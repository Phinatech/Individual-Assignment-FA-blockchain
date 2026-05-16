/* ============================================================
 *  crypto.c  --  SHA-256 + ECDSA wrappers over OpenSSL 3.x
 *
 *  Uses the modern EVP_* APIs so the code compiles cleanly
 *  on OpenSSL 3.0+ without deprecation warnings.
 * ============================================================ */
#include "crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/ec.h>
#include <openssl/sha.h>

/* The admin keypair, kept loaded for the life of the program. */
static EVP_PKEY *g_admin_priv = NULL;   /* full keypair (private + public) */
static EVP_PKEY *g_admin_pub  = NULL;   /* public key only                 */

/* ------------------------------------------------------------ helpers */
static void hex_encode(const unsigned char *in, size_t in_len,
                       char *out_hex)
{
    static const char tab[] = "0123456789abcdef";
    for (size_t i = 0; i < in_len; i++) {
        out_hex[2 * i]     = tab[(in[i] >> 4) & 0xF];
        out_hex[2 * i + 1] = tab[in[i] & 0xF];
    }
    out_hex[2 * in_len] = '\0';
}

static void ensure_key_dir(void)
{
    struct stat st;
    if (stat(KEY_DIR, &st) != 0) {
        if (mkdir(KEY_DIR, 0700) != 0 && errno != EEXIST) {
            fprintf(stderr,
                    "WARN : Could not create '%s' directory: %s\n",
                    KEY_DIR, strerror(errno));
        }
    }
}

static void crypto_print_error(const char *where)
{
    fprintf(stderr, "OpenSSL error in %s:\n", where);
    ERR_print_errors_fp(stderr);
}

/* ------------------------------------------------------------ key I/O */
static int save_admin_keys(EVP_PKEY *pkey)
{
    ensure_key_dir();

    FILE *fp = fopen(KEY_FILE_PRIV, "wb");
    if (!fp) {
        perror("fopen(priv)");
        return -1;
    }
    if (!PEM_write_PrivateKey(fp, pkey, NULL, NULL, 0, NULL, NULL)) {
        crypto_print_error("PEM_write_PrivateKey");
        fclose(fp);
        return -1;
    }
    fclose(fp);
    chmod(KEY_FILE_PRIV, 0600);   /* readable only by owner */

    fp = fopen(KEY_FILE_PUB, "wb");
    if (!fp) {
        perror("fopen(pub)");
        return -1;
    }
    if (!PEM_write_PUBKEY(fp, pkey)) {
        crypto_print_error("PEM_write_PUBKEY");
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static EVP_PKEY *generate_admin_keypair(void)
{
    /* OpenSSL 3.0 high-level helper: generates a P-256 EC keypair. */
    EVP_PKEY *pkey = EVP_EC_gen("P-256");
    if (!pkey) {
        crypto_print_error("EVP_EC_gen");
        return NULL;
    }
    return pkey;
}

static EVP_PKEY *load_admin_private(void)
{
    FILE *fp = fopen(KEY_FILE_PRIV, "rb");
    if (!fp) return NULL;
    EVP_PKEY *pkey = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
    fclose(fp);
    return pkey;
}

static EVP_PKEY *load_admin_public(void)
{
    FILE *fp = fopen(KEY_FILE_PUB, "rb");
    if (!fp) return NULL;
    EVP_PKEY *pkey = PEM_read_PUBKEY(fp, NULL, NULL, NULL);
    fclose(fp);
    return pkey;
}

/* ------------------------------------------------------------ API */
int crypto_init(void)
{
    g_admin_priv = load_admin_private();
    g_admin_pub  = load_admin_public();

    if (!g_admin_priv || !g_admin_pub) {
        if (g_admin_priv) { EVP_PKEY_free(g_admin_priv); g_admin_priv = NULL; }
        if (g_admin_pub)  { EVP_PKEY_free(g_admin_pub);  g_admin_pub  = NULL; }

        printf("INFO : No admin keypair found.  Generating a fresh "
               "ECDSA P-256 keypair...\n");
        g_admin_priv = generate_admin_keypair();
        if (!g_admin_priv) return -1;
        if (save_admin_keys(g_admin_priv) != 0) return -1;
        g_admin_pub = load_admin_public();
        if (!g_admin_pub) return -1;
        printf("INFO : Admin keys stored in '%s/'.\n", KEY_DIR);
    }
    return 0;
}

void crypto_cleanup(void)
{
    if (g_admin_priv) { EVP_PKEY_free(g_admin_priv); g_admin_priv = NULL; }
    if (g_admin_pub)  { EVP_PKEY_free(g_admin_pub);  g_admin_pub  = NULL; }
}

void sha256_hex(const unsigned char *data, size_t len,
                char out_hex[HASH_HEX_LEN])
{
    unsigned char raw[EVP_MAX_MD_SIZE];
    unsigned int  raw_len = 0;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        crypto_print_error("EVP_MD_CTX_new");
        memset(out_hex, '0', HASH_HEX_LEN - 1);
        out_hex[HASH_HEX_LEN - 1] = '\0';
        return;
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, data, len) != 1 ||
        EVP_DigestFinal_ex(ctx, raw, &raw_len) != 1) {
        crypto_print_error("EVP_Digest*");
        EVP_MD_CTX_free(ctx);
        memset(out_hex, '0', HASH_HEX_LEN - 1);
        out_hex[HASH_HEX_LEN - 1] = '\0';
        return;
    }
    EVP_MD_CTX_free(ctx);
    hex_encode(raw, raw_len, out_hex);
}

size_t sign_data(const unsigned char *data, size_t len,
                 unsigned char *sig_out, size_t sig_max)
{
    if (!g_admin_priv) return 0;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return 0;

    size_t sig_len = 0;
    int ok = 0;
    do {
        if (EVP_DigestSignInit(ctx, NULL, EVP_sha256(),
                               NULL, g_admin_priv) != 1) break;
        if (EVP_DigestSignUpdate(ctx, data, len) != 1) break;
        /* First call computes the required size. */
        if (EVP_DigestSignFinal(ctx, NULL, &sig_len) != 1) break;
        if (sig_len > sig_max) {
            fprintf(stderr,
                    "ERROR: signature too long (%zu > %zu)\n",
                    sig_len, sig_max);
            break;
        }
        if (EVP_DigestSignFinal(ctx, sig_out, &sig_len) != 1) break;
        ok = 1;
    } while (0);

    if (!ok) {
        crypto_print_error("sign_data");
        sig_len = 0;
    }
    EVP_MD_CTX_free(ctx);
    return sig_len;
}

int verify_data(const unsigned char *data, size_t len,
                const unsigned char *sig, size_t sig_len)
{
    if (!g_admin_pub) return -1;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return -1;

    int result = -1;
    do {
        if (EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(),
                                 NULL, g_admin_pub) != 1) break;
        if (EVP_DigestVerifyUpdate(ctx, data, len) != 1) break;
        int rc = EVP_DigestVerifyFinal(ctx, sig, sig_len);
        result = (rc == 1) ? 1 : 0;
    } while (0);

    EVP_MD_CTX_free(ctx);
    return result;
}

void admin_pubkey_fingerprint(char out_hex[HASH_HEX_LEN])
{
    if (!g_admin_pub) {
        memset(out_hex, '0', HASH_HEX_LEN - 1);
        out_hex[HASH_HEX_LEN - 1] = '\0';
        return;
    }
    unsigned char *buf = NULL;
    int n = i2d_PUBKEY(g_admin_pub, &buf);
    if (n <= 0 || !buf) {
        memset(out_hex, '0', HASH_HEX_LEN - 1);
        out_hex[HASH_HEX_LEN - 1] = '\0';
        return;
    }
    sha256_hex(buf, (size_t)n, out_hex);
    OPENSSL_free(buf);
}
