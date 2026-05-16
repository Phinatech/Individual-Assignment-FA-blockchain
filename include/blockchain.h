/* ============================================================
 *  blockchain.h  --  Blockchain data structure and operations
 * ============================================================ */
#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <time.h>
#include <stddef.h>

#include "student.h"
#include "crypto.h"

#define STATUS_LEN 10            /* "PRESENT" / "ABSENT" / "LATE" */

/* The Block matches the structure mandated by the assignment.
 *
 *   index         : position in the chain (0 = Genesis)
 *   timestamp     : Unix time at block creation
 *   student_id    : validated against the registry before block creation
 *   full_name     : copied from the registry at marking time
 *   course_code   : course attendance is being recorded for
 *   status        : PRESENT | ABSENT | LATE
 *   previous_hash : SHA-256 hex of the previous block (64 zeros for genesis)
 *   signature     : ECDSA P-256 signature of the block content
 *   sig_len       : actual signature byte count (DER is variable length)
 *   hash          : SHA-256 of content || signature
 */
typedef struct {
    int            index;
    time_t         timestamp;
    char           student_id[STUDENT_ID_LEN];
    char           full_name[FULL_NAME_LEN];
    char           course_code[COURSE_CODE_LEN];
    char           status[STATUS_LEN];
    char           previous_hash[HASH_HEX_LEN];
    unsigned char  signature[SIG_MAX_LEN];
    size_t         sig_len;
    char           hash[HASH_HEX_LEN];
} Block;

typedef struct BlockNode {
    Block             block;
    struct BlockNode *next;
} BlockNode;

typedef struct {
    BlockNode *head;
    BlockNode *tail;
    int        length;
} Blockchain;

/* ------------------------------------------------------------ life-cycle */
void chain_init   (Blockchain *chain);
void chain_destroy(Blockchain *chain);

/* Create the Genesis block (index 0) if the chain is empty.
 * Returns 0 on success, -1 on failure. */
int  chain_create_genesis(Blockchain *chain);

/* Append a new attendance block (status must be PRESENT/ABSENT/LATE).
 * The block is signed and hashed in place.
 * Returns 0 on success, -1 on failure. */
int  chain_add_block(Blockchain *chain,
                     const Student *student,
                     const char    *status);

/* Walk the entire chain and verify
 *    (a) each block's hash matches its computed hash,
 *    (b) each previous_hash matches the prior block's hash,
 *    (c) each signature verifies against the admin public key.
 * Returns 0 on a fully valid chain; -1 if any check fails.
 * Diagnostic output is printed for the user. */
int  chain_validate(const Blockchain *chain);

/* Print all attendance records in tabular form. */
void chain_print(const Blockchain *chain);

/* ------------------------------------------------------------ hashing */
/* Serialise the deterministic content of a block (everything that the
 * hash and signature must cover) into a delimited byte buffer.
 * Returns the number of bytes written (excluding terminating NUL). */
size_t block_serialize_content(const Block *b,
                               unsigned char *buf, size_t bufsz);

/* Compute the SHA-256 hex hash over content || signature. */
void block_compute_hash(const Block *b, char out_hex[HASH_HEX_LEN]);

/* ------------------------------------------------------------ tamper test */
/* Programmatically mutate the status field of the block at `target_index`
 * WITHOUT recomputing its hash.  This intentionally breaks integrity
 * so the next validate() call fails -- used by the CLI tamper demo. */
int chain_tamper(Blockchain *chain, int target_index, const char *new_status);

/* Lookup a block by index (returns NULL if not found). */
const Block *chain_get_block(const Blockchain *chain, int index);

#endif /* BLOCKCHAIN_H */
