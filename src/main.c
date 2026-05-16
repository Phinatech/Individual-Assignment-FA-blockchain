/* ============================================================
 *  main.c  --  Blockchain Attendance Tracking System
 *              CLI entry point + main menu loop.
 *
 *  Build:    make
 *  Run :     ./attendance
 * ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "student.h"
#include "blockchain.h"
#include "storage.h"
#include "crypto.h"

#define STUDENTS_FILE "students.txt"

/* ------------------------------------------------------------ input */
static void chomp(char *s)
{
    if (!s) return;
    size_t L = strlen(s);
    while (L > 0 && (s[L-1] == '\n' || s[L-1] == '\r' || s[L-1] == ' '))
        s[--L] = '\0';
}

static void prompt(const char *p, char *out, size_t out_sz)
{
    printf("%s", p);
    fflush(stdout);
    if (!fgets(out, (int)out_sz, stdin)) {
        out[0] = '\0';
        return;
    }
    chomp(out);
}

static int is_valid_status(const char *s)
{
    return (strcasecmp(s, "PRESENT") == 0 ||
            strcasecmp(s, "ABSENT")  == 0 ||
            strcasecmp(s, "LATE")    == 0);
}

/* ------------------------------------------------------------ banner */
static void banner(void)
{
    printf("\n");
    printf("  ============================================================\n");
    printf("    BLOCKCHAIN ATTENDANCE TRACKING SYSTEM\n");
    printf("    Cryptographic ledger using SHA-256 + ECDSA P-256\n");
    printf("  ============================================================\n");

    char fp[HASH_HEX_LEN];
    admin_pubkey_fingerprint(fp);
    printf("    Admin pubkey fingerprint: %.16s...%s\n", fp, fp + 48);
    printf("  ============================================================\n\n");
}

static void menu(void)
{
    printf("  ---------------- MENU ----------------\n");
    printf("   1) View student registry\n");
    printf("   2) Mark attendance\n");
    printf("   3) View attendance records\n");
    printf("   4) Validate chain\n");
    printf("   5) Tamper test (demonstration)\n");
    printf("   6) Save chain now\n");
    printf("   0) Exit (auto-saves)\n");
    printf("  ---------------------------------------\n");
}

/* ------------------------------------------------------------ handlers */
static void handle_mark(const StudentRegistry *reg, Blockchain *chain)
{
    char id[64], status[32];
    prompt("  Student ID: ", id, sizeof id);
    if (id[0] == '\0') {
        printf("  (cancelled)\n");
        return;
    }

    const Student *s = registry_find(reg, id);
    if (!s) {
        printf("\n  ERROR: Student ID not found.\n\n");   /* required message */
        return;
    }

    prompt("  Status (PRESENT / ABSENT / LATE): ", status, sizeof status);
    if (!is_valid_status(status)) {
        printf("\n  ERROR: status must be PRESENT, ABSENT, or LATE.\n\n");
        return;
    }

    if (chain_add_block(chain, s, status) == 0) {
        const Block *b = chain->tail ? &chain->tail->block : NULL;
        printf("\n  OK   : Attendance recorded for %s (%s) as %s.\n",
               s->full_name, s->student_id, status);
        if (b) {
            printf("         Block #%d, hash %.16s...\n\n",
                   b->index, b->hash);
        }
        /* Auto-save after every successful append. */
        chain_save(chain, CHAIN_FILE);
    } else {
        printf("\n  ERROR: failed to append block.\n\n");
    }
}

static void handle_tamper(Blockchain *chain)
{
    if (chain->length < 2) {
        printf("  Need at least one non-genesis block to demonstrate "
               "tampering.\n");
        return;
    }
    char idx_s[32], new_status[32];
    prompt("  Block index to tamper: ", idx_s, sizeof idx_s);
    int idx = atoi(idx_s);
    if (idx <= 0 || idx >= chain->length) {
        printf("  ERROR: index out of range (1 .. %d)\n", chain->length - 1);
        return;
    }
    prompt("  New status to inject: ", new_status, sizeof new_status);
    if (new_status[0] == '\0') {
        printf("  (cancelled)\n");
        return;
    }
    chain_tamper(chain, idx, new_status);
    printf("\n  >> Run option 4 (Validate chain) to see the integrity "
           "check FAIL.\n\n");
}

/* ------------------------------------------------------------ main */
int main(void)
{
    /* 1. Initialise OpenSSL and load/create the admin keypair. */
    if (crypto_init() != 0) {
        fprintf(stderr, "FATAL: cryptography sub-system failed to start.\n");
        return EXIT_FAILURE;
    }

    /* 2. Load the student registry.  Refuse to run without it. */
    StudentRegistry reg;
    if (registry_load(&reg, STUDENTS_FILE) < 0) {
        fprintf(stderr,
                "FATAL: cannot proceed without a valid '%s'.\n",
                STUDENTS_FILE);
        crypto_cleanup();
        return EXIT_FAILURE;
    }

    /* 3. Load the blockchain ledger, or create a genesis block. */
    Blockchain chain;
    chain_init(&chain);
    chain_load(&chain, CHAIN_FILE);
    if (chain.length == 0) {
        chain_create_genesis(&chain);
        chain_save(&chain, CHAIN_FILE);
    } else {
        printf("INFO : Loaded %d block(s) from '%s'.\n",
               chain.length, CHAIN_FILE);
    }

    banner();
    printf("Loaded %d student(s) from '%s'.\n\n", reg.count, STUDENTS_FILE);

    /* 4. Main loop */
    char choice[16];
    int running = 1;
    while (running) {
        menu();
        prompt("  > ", choice, sizeof choice);
        switch (choice[0]) {
            case '1': registry_print(&reg);                break;
            case '2': handle_mark(&reg, &chain);           break;
            case '3': chain_print(&chain);                 break;
            case '4': chain_validate(&chain);              break;
            case '5': handle_tamper(&chain);               break;
            case '6':
                if (chain_save(&chain, CHAIN_FILE) == 0)
                    printf("  Saved %d block(s) to '%s'.\n\n",
                           chain.length, CHAIN_FILE);
                break;
            case '0':
            case 'q': case 'Q':
                running = 0;
                break;
            default:
                printf("  (unrecognised choice)\n");
                break;
        }
    }

    chain_save(&chain, CHAIN_FILE);
    printf("\nGoodbye. Chain saved to '%s'.\n", CHAIN_FILE);

    chain_destroy(&chain);
    crypto_cleanup();
    return EXIT_SUCCESS;
}
