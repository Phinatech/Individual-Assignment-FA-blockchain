# Blockchain Attendance Tracking System

A tamper-evident attendance ledger written in C. Every attendance record is
stored as a block in a chain, linked by SHA-256 hashes and authenticated with
ECDSA P-256 digital signatures. Once a record is in the chain, modifying it
breaks the cryptographic links and validation will fail.

---

## 1. Features

- **Student registry** loaded from `students.txt`; every attendance attempt
  is validated against it before a block can be appended.
- **Genesis block** at index 0 with `previous_hash` = 64 zeros.
- **SHA-256 hashing** of each block's canonical content + signature.
- **ECDSA P-256 digital signatures** over each block, using an admin
  keypair auto-generated on first run and stored in `keys/`.
- **File persistence** of the entire ledger to `attendance.dat` (text format,
  hex-encoded signature, one block per line).
- **Chain validation** that checks all three integrity layers:
  hash recomputation, `previous_hash` linkage, and signature verification.
- **Tamper-test command** that intentionally mutates a block in place so
  you can watch validation fail.
- **CLI menu** with seven actions; auto-saves the chain after every change.

---

## 2. Requirements

| Component | Version | Notes |
| --------- | ------- | ----- |
| GCC (or clang) | C11 capable | `-std=c11` is set in the Makefile |
| GNU Make | any recent | |
| OpenSSL development headers | **3.0+** | `libssl-dev` on Debian/Ubuntu |
| POSIX OS | Linux, macOS, WSL | Uses `mkdir`, `chmod`, `localtime_r` |

### Installing OpenSSL dev headers

```bash
# Ubuntu / Debian / WSL
sudo apt-get install -y libssl-dev

# Fedora / RHEL
sudo dnf install -y openssl-devel

# macOS (Homebrew)
brew install openssl@3
# and add to your shell:
#   export PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig"
```

---

## 3. Build

```bash
make            # produces ./attendance
make run        # build then launch
make clean      # remove build/ and the binary
make distclean  # also remove keys/ and attendance.dat
```

The provided `Makefile` compiles with strict warnings
(`-Wall -Wextra -Wpedantic -Wshadow`) and links against `-lssl -lcrypto`.

---

## 4. First run

On its first run the program:

1. Creates `keys/` and generates an ECDSA P-256 admin keypair
   (`admin_priv.pem`, `admin_pub.pem`).
2. Loads `students.txt`.
3. Creates the **genesis block** and writes `attendance.dat`.

Subsequent runs reuse the same keypair and re-load the existing chain.

---

## 5. CLI menu

```
1) View student registry        -- prints the loaded students table
2) Mark attendance              -- asks for ID + status, validates, signs, appends
3) View attendance records      -- prints every block (header + body)
4) Validate chain               -- runs the 3-layer integrity check
5) Tamper test (demonstration)  -- mutates a block in place to break the chain
6) Save chain now               -- force-write attendance.dat
0) Exit                         -- auto-saves the chain
```

Valid status values: `PRESENT`, `ABSENT`, `LATE` (case-insensitive).

---

## 6. Layout

```
.
├── Makefile
├── README.md
├── students.txt            <- the registry (edit this)
├── attendance.dat          <- the persisted blockchain (created on first run)
├── keys/                   <- admin keypair (created on first run)
│   ├── admin_priv.pem
│   └── admin_pub.pem
├── include/
│   ├── blockchain.h
│   ├── crypto.h
│   ├── storage.h
│   └── student.h
├── src/
│   ├── blockchain.c        <- block creation, hashing, validation
│   ├── crypto.c            <- SHA-256 + ECDSA wrappers (OpenSSL 3 EVP API)
│   ├── main.c              <- CLI menu loop
│   ├── storage.c           <- file persistence
│   └── student.c           <- registry loader & lookup
└── docs/
    ├── system_design.svg   <- architecture diagram
    ├── TECHNICAL_REPORT.docx
    ├── demo_session.txt    <- captured demo output
    └── demo_tamper.txt     <- captured tamper-detection output
```

---

## 7. Storage format (`attendance.dat`)

One block per line, pipe-delimited:

```
index|timestamp|student_id|full_name|course_code|status|previous_hash|signature_hex|hash
```

`signature_hex` is the DER-encoded ECDSA signature, hex-encoded so the
file is fully text-safe and editable (useful for tamper demos).

---

## 8. Demonstrating tamper detection

Two ways to break the chain on purpose:

**A. From inside the program** — pick option `5`, give a block index and
a fake status. Option `4` will then report:

```
FAIL : Block #N hash mismatch.
FAIL : Block #N signature verification failed.
BAD  : Chain is INVALID.  Tampering detected.
```

**B. From outside the program** — edit `attendance.dat` in a text editor
and change a status field (for example, change `ABSENT` to `PRESENT`).
On the next launch, option `4` will catch it the same way.

To restore a clean state after a tamper test:

```bash
make distclean && make
```

---

## 9. Security notes

- The admin private key is the **only** thing that can sign new blocks.
  It is stored on disk with `0600` permissions and only readable by the
  account that created it.
- SHA-256 covers **content + signature**, so neither can be altered
  independently without detection.
- The chain's `previous_hash` linkage means tampering with an old block
  invalidates every block after it as well.
- For a stronger production system, replace the single admin key with
  per-instructor or per-student keypairs (see the Technical Report for
  details).

---

## 10. License

Released for educational use as part of the ALU Operating Systems &
Cryptography lab assignment.
