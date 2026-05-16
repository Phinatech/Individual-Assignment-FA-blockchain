/* ============================================================
 *  storage.c  --  Blockchain persistence (file-backed ledger)
 * ============================================================ */
#include "storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ------------------------------------------------------------ hex */
static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void hex_encode(const unsigned char *in, size_t in_len, char *out)
{
    static const char tab[] = "0123456789abcdef";
    for (size_t i = 0; i < in_len; i++) {
        out[2 * i]     = tab[(in[i] >> 4) & 0xF];
        out[2 * i + 1] = tab[in[i] & 0xF];
    }
    out[2 * in_len] = '\0';
}

static int hex_decode(const char *hex, unsigned char *out, size_t out_max,
                      size_t *out_len)
{
    size_t hex_len = strlen(hex);
    if (hex_len % 2 != 0) return -1;
    size_t need = hex_len / 2;
    if (need > out_max) return -1;
    for (size_t i = 0; i < need; i++) {
        int h = hex_val(hex[2 * i]);
        int l = hex_val(hex[2 * i + 1]);
        if (h < 0 || l < 0) return -1;
        out[i] = (unsigned char)((h << 4) | l);
    }
    *out_len = need;
    return 0;
}

/* ------------------------------------------------------------ save */
int chain_save(const Blockchain *chain, const char *filename)
{
    if (!chain || !filename) return -1;
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "ERROR: cannot write '%s': %s\n",
                filename, strerror(errno));
        return -1;
    }

    for (BlockNode *n = chain->head; n; n = n->next) {
        const Block *b = &n->block;
        char sig_hex[SIG_MAX_LEN * 2 + 1];
        hex_encode(b->signature, b->sig_len, sig_hex);

        fprintf(fp, "%d|%lld|%s|%s|%s|%s|%s|%s|%s\n",
                b->index,
                (long long)b->timestamp,
                b->student_id,
                b->full_name,
                b->course_code,
                b->status,
                b->previous_hash,
                sig_hex,
                b->hash);
    }
    fclose(fp);
    return 0;
}

/* ------------------------------------------------------------ load */
static char *next_field(char **cursor)
{
    if (!cursor || !*cursor) return NULL;
    char *start = *cursor;
    char *sep   = strchr(start, '|');
    if (sep) {
        *sep = '\0';
        *cursor = sep + 1;
    } else {
        *cursor = NULL;
    }
    return start;
}

static void chain_append_raw(Blockchain *chain, const Block *b)
{
    BlockNode *node = (BlockNode *)calloc(1, sizeof(BlockNode));
    if (!node) return;
    node->block = *b;
    node->next  = NULL;
    if (chain->tail) chain->tail->next = node;
    else             chain->head       = node;
    chain->tail = node;
    chain->length++;
}

int chain_load(Blockchain *chain, const char *filename)
{
    if (!chain) return -1;
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;            /* no file -> empty chain, not an error */

    char line[2048];
    int  line_no = 0;
    while (fgets(line, sizeof line, fp)) {
        line_no++;
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r'))
            line[--L] = '\0';
        if (L == 0) continue;

        Block b;
        memset(&b, 0, sizeof b);

        char *cur = line;
        char *idx_s   = next_field(&cur);
        char *ts_s    = next_field(&cur);
        char *id      = next_field(&cur);
        char *name    = next_field(&cur);
        char *code    = next_field(&cur);
        char *status  = next_field(&cur);
        char *prev    = next_field(&cur);
        char *sig_hex = next_field(&cur);
        char *hash    = next_field(&cur);

        if (!idx_s || !ts_s || !id || !name || !code || !status ||
            !prev || !sig_hex || !hash) {
            fprintf(stderr,
                    "ERROR: chain file line %d malformed (skipped)\n",
                    line_no);
            continue;
        }

        b.index     = atoi(idx_s);
        b.timestamp = (time_t)atoll(ts_s);
        strncpy(b.student_id,    id,     STUDENT_ID_LEN  - 1);
        strncpy(b.full_name,     name,   FULL_NAME_LEN   - 1);
        strncpy(b.course_code,   code,   COURSE_CODE_LEN - 1);
        strncpy(b.status,        status, STATUS_LEN      - 1);
        strncpy(b.previous_hash, prev,   HASH_HEX_LEN    - 1);
        strncpy(b.hash,          hash,   HASH_HEX_LEN    - 1);

        if (hex_decode(sig_hex, b.signature, SIG_MAX_LEN, &b.sig_len) != 0) {
            fprintf(stderr,
                    "ERROR: chain file line %d bad signature hex\n",
                    line_no);
            continue;
        }
        chain_append_raw(chain, &b);
    }
    fclose(fp);
    return 0;
}
