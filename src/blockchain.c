/* ============================================================
 *  blockchain.c  --  Blockchain implementation
 * ============================================================ */
#include "blockchain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ----------------------------------------------------------------
 *  life-cycle
 * ---------------------------------------------------------------- */
void chain_init(Blockchain *chain)
{
    if (!chain) return;
    chain->head   = NULL;
    chain->tail   = NULL;
    chain->length = 0;
}

void chain_destroy(Blockchain *chain)
{
    if (!chain) return;
    BlockNode *n = chain->head;
    while (n) {
        BlockNode *next = n->next;
        free(n);
        n = next;
    }
    chain->head = chain->tail = NULL;
    chain->length = 0;
}

static BlockNode *node_new(void)
{
    BlockNode *n = (BlockNode *)calloc(1, sizeof(BlockNode));
    return n;
}

static void chain_append_node(Blockchain *chain, BlockNode *node)
{
    if (chain->tail) chain->tail->next = node;
    else             chain->head       = node;
    chain->tail = node;
    chain->length++;
}

const Block *chain_get_block(const Blockchain *chain, int index)
{
    if (!chain) return NULL;
    for (BlockNode *n = chain->head; n; n = n->next)
        if (n->block.index == index) return &n->block;
    return NULL;
}

/* ----------------------------------------------------------------
 *  hashing / signing helpers
 * ---------------------------------------------------------------- */
size_t block_serialize_content(const Block *b,
                               unsigned char *buf, size_t bufsz)
{
    /* A simple pipe-delimited canonical form.
     * Order is fixed and identical for hashing and signing. */
    int n = snprintf((char *)buf, bufsz,
                     "%d|%lld|%s|%s|%s|%s|%s",
                     b->index,
                     (long long)b->timestamp,
                     b->student_id,
                     b->full_name,
                     b->course_code,
                     b->status,
                     b->previous_hash);
    if (n < 0 || (size_t)n >= bufsz) return 0;
    return (size_t)n;
}

void block_compute_hash(const Block *b, char out_hex[HASH_HEX_LEN])
{
    /* hash = SHA256( serialized_content || signature_bytes ) */
    unsigned char buf[1024];
    size_t n = block_serialize_content(b, buf, sizeof buf);
    if (n == 0) {
        memset(out_hex, '0', HASH_HEX_LEN - 1);
        out_hex[HASH_HEX_LEN - 1] = '\0';
        return;
    }
    /* Append signature bytes if room is available */
    if (b->sig_len > 0 && n + b->sig_len <= sizeof buf) {
        memcpy(buf + n, b->signature, b->sig_len);
        n += b->sig_len;
    }
    sha256_hex(buf, n, out_hex);
}

/* ----------------------------------------------------------------
 *  block construction
 * ---------------------------------------------------------------- */
int chain_create_genesis(Blockchain *chain)
{
    if (!chain) return -1;
    if (chain->head != NULL) return 0;          /* already exists */

    BlockNode *node = node_new();
    if (!node) return -1;

    Block *b = &node->block;
    b->index     = 0;
    b->timestamp = time(NULL);
    strcpy(b->student_id,  "GENESIS");
    strcpy(b->full_name,   "Genesis Block");
    strcpy(b->course_code, "N/A");
    strcpy(b->status,      "GENESIS");

    /* previous_hash = 64 zeros */
    memset(b->previous_hash, '0', HASH_HEX_LEN - 1);
    b->previous_hash[HASH_HEX_LEN - 1] = '\0';

    /* Sign the serialised content. */
    unsigned char buf[1024];
    size_t n = block_serialize_content(b, buf, sizeof buf);
    b->sig_len = sign_data(buf, n, b->signature, SIG_MAX_LEN);

    block_compute_hash(b, b->hash);
    chain_append_node(chain, node);

    printf("INFO : Genesis block created (index 0, hash %.16s...).\n",
           b->hash);
    return 0;
}

int chain_add_block(Blockchain *chain,
                    const Student *student,
                    const char    *status)
{
    if (!chain || !student || !status) return -1;
    if (chain->tail == NULL) {
        fprintf(stderr,
                "ERROR: Genesis block not present; cannot append.\n");
        return -1;
    }

    BlockNode *node = node_new();
    if (!node) return -1;

    Block *b = &node->block;
    b->index     = chain->length;                   /* next index */
    b->timestamp = time(NULL);

    /* Copy from validated registry record (zero buffer then bounded copy
     * to guarantee NUL termination without compiler truncation warnings). */
    memset(b->student_id,  0, STUDENT_ID_LEN);
    memset(b->full_name,   0, FULL_NAME_LEN);
    memset(b->course_code, 0, COURSE_CODE_LEN);
    memcpy(b->student_id,  student->student_id,
           strnlen(student->student_id,  STUDENT_ID_LEN  - 1));
    memcpy(b->full_name,   student->full_name,
           strnlen(student->full_name,   FULL_NAME_LEN   - 1));
    memcpy(b->course_code, student->course_code,
           strnlen(student->course_code, COURSE_CODE_LEN - 1));

    /* Status -- uppercase + bounded */
    strncpy(b->status, status, STATUS_LEN - 1);
    b->status[STATUS_LEN - 1] = '\0';
    for (char *p = b->status; *p; ++p)
        if (*p >= 'a' && *p <= 'z') *p -= 32;

    /* previous_hash from the tail block */
    memcpy(b->previous_hash, chain->tail->block.hash, HASH_HEX_LEN);

    /* Sign content with admin private key */
    unsigned char buf[1024];
    size_t n = block_serialize_content(b, buf, sizeof buf);
    if (n == 0) {
        free(node);
        fprintf(stderr, "ERROR: failed to serialise block\n");
        return -1;
    }
    b->sig_len = sign_data(buf, n, b->signature, SIG_MAX_LEN);
    if (b->sig_len == 0) {
        free(node);
        fprintf(stderr, "ERROR: signing failed\n");
        return -1;
    }

    block_compute_hash(b, b->hash);
    chain_append_node(chain, node);
    return 0;
}

/* ----------------------------------------------------------------
 *  validation
 * ---------------------------------------------------------------- */
int chain_validate(const Blockchain *chain)
{
    if (!chain || !chain->head) {
        printf("Chain is empty.\n");
        return -1;
    }

    int failed = 0;
    const Block *prev = NULL;

    for (BlockNode *n = chain->head; n; n = n->next) {
        const Block *b = &n->block;

        /* 1. Recompute the hash from content+signature. */
        char recomputed[HASH_HEX_LEN];
        block_compute_hash(b, recomputed);
        if (strcmp(recomputed, b->hash) != 0) {
            printf("FAIL : Block #%d hash mismatch.\n", b->index);
            printf("       stored:     %s\n", b->hash);
            printf("       recomputed: %s\n", recomputed);
            failed = 1;
        }

        /* 2. Verify previous_hash linkage. */
        if (prev) {
            if (strcmp(b->previous_hash, prev->hash) != 0) {
                printf("FAIL : Block #%d previous_hash does not match "
                       "block #%d's hash.\n", b->index, prev->index);
                failed = 1;
            }
        } else {
            /* Genesis must point to all zeros. */
            for (int i = 0; i < HASH_HEX_LEN - 1; i++) {
                if (b->previous_hash[i] != '0') {
                    printf("FAIL : Genesis previous_hash is not all zeros.\n");
                    failed = 1;
                    break;
                }
            }
        }

        /* 3. Verify ECDSA signature over the canonical content. */
        unsigned char buf[1024];
        size_t cn = block_serialize_content(b, buf, sizeof buf);
        int sig_ok = verify_data(buf, cn, b->signature, b->sig_len);
        if (sig_ok != 1) {
            printf("FAIL : Block #%d signature verification failed.\n",
                   b->index);
            failed = 1;
        }

        prev = b;
    }

    if (!failed) {
        printf("OK   : Chain is VALID (%d block%s, all hashes & "
               "signatures verified).\n",
               chain->length, chain->length == 1 ? "" : "s");
        return 0;
    }
    printf("BAD  : Chain is INVALID.  Tampering detected.\n");
    return -1;
}

/* ----------------------------------------------------------------
 *  printing
 * ---------------------------------------------------------------- */
static void format_time(time_t t, char *out, size_t out_sz)
{
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(out, out_sz, "%Y-%m-%d %H:%M:%S", &tmv);
}

void chain_print(const Blockchain *chain)
{
    if (!chain || !chain->head) {
        printf("(chain is empty)\n");
        return;
    }
    printf("\n=============================================================="
           "==================\n");
    printf(" ATTENDANCE LEDGER  (%d block%s)\n",
           chain->length, chain->length == 1 ? "" : "s");
    printf("=============================================================="
           "==================\n");

    for (BlockNode *n = chain->head; n; n = n->next) {
        const Block *b = &n->block;

        /* Verify this block's signature for display purposes. */
        unsigned char buf[1024];
        size_t cn = block_serialize_content(b, buf, sizeof buf);
        int sig_ok = verify_data(buf, cn, b->signature, b->sig_len);

        char ts[32];
        format_time(b->timestamp, ts, sizeof ts);

        printf("\n[Block %d]\n", b->index);
        printf("  Timestamp     : %s\n", ts);
        printf("  Student ID    : %s\n", b->student_id);
        printf("  Full Name     : %s\n", b->full_name);
        printf("  Course        : %s\n", b->course_code);
        printf("  Status        : %s\n", b->status);
        printf("  Previous Hash : %.16s...%s\n",
               b->previous_hash, b->previous_hash + 48);
        printf("  Block Hash    : %.16s...%s\n",
               b->hash, b->hash + 48);
        printf("  Signature     : %zu bytes, %s\n",
               b->sig_len,
               sig_ok == 1 ? "VALID" : "INVALID");
    }
    printf("\n=============================================================="
           "==================\n\n");
}

/* ----------------------------------------------------------------
 *  tamper test
 * ---------------------------------------------------------------- */
int chain_tamper(Blockchain *chain, int target_index, const char *new_status)
{
    if (!chain || !new_status) return -1;
    for (BlockNode *n = chain->head; n; n = n->next) {
        if (n->block.index == target_index) {
            printf("TAMPER: Mutating block #%d status from '%s' -> '%s' "
                   "WITHOUT recomputing its hash.\n",
                   target_index, n->block.status, new_status);
            strncpy(n->block.status, new_status, STATUS_LEN - 1);
            n->block.status[STATUS_LEN - 1] = '\0';
            return 0;
        }
    }
    printf("ERROR: No block with index %d.\n", target_index);
    return -1;
}
