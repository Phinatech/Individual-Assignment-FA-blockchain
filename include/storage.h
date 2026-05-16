/* ============================================================
 *  storage.h  --  Blockchain persistence (file-backed ledger)
 *
 *  The chain is serialised as a hex-encoded, pipe-delimited
 *  text file -- one line per block.  The signature bytes are
 *  hex-encoded so the file is fully text-safe (and editable,
 *  which is useful for tamper demonstrations).
 *
 *  Line format:
 *    index|timestamp|student_id|full_name|course_code|status|
 *    previous_hash|sig_hex|hash
 * ============================================================ */
#ifndef STORAGE_H
#define STORAGE_H

#include "blockchain.h"

#define CHAIN_FILE "attendance.dat"

/* Returns 0 on success, -1 on failure. */
int chain_save(const Blockchain *chain, const char *filename);

/* Loads a chain from file.  If the file does not exist, leaves
 * the chain empty and returns 0.  Returns -1 on parse error. */
int chain_load(Blockchain *chain, const char *filename);

#endif /* STORAGE_H */
