# =====================================================================
#  Makefile for Blockchain Attendance Tracking System
# =====================================================================

OS := $(shell uname -s 2>/dev/null || echo NOT_LINUX)

ifneq ($(OS),Linux)
$(error ERROR: This project requires Linux. Detected: "$(OS)".)
endif

HAS_GCC    := $(shell command -v gcc 2>/dev/null)
HAS_PKGCFG := $(shell command -v pkg-config 2>/dev/null)
HAS_SSL    := $(shell pkg-config --exists openssl 2>/dev/null && echo yes || echo no)
HAS_SSLHDR := $(shell test -f /usr/include/openssl/evp.h 2>/dev/null && echo yes || echo no)

CC      := gcc
CSTD    := -std=c11
WARN    := -Wall -Wextra -Wpedantic -Wshadow -Wno-unused-parameter
OPT     := -O2
INC     := -Iinclude
DEFS    := -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
CFLAGS  := $(CSTD) $(WARN) $(OPT) $(INC) $(DEFS)
LDLIBS  := -lssl -lcrypto

SRCDIR  := src
OBJDIR  := build
BIN     := attendance

SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

.PHONY: all run clean distclean check-env

all: check-env $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)
	@echo ""
	@echo "  Build successful! Run with:  ./$(BIN)"
	@echo ""

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

run: all
	./$(BIN)

clean:
	rm -rf $(OBJDIR) $(BIN)

distclean: clean
	rm -rf keys attendance.dat

check-env:
	@echo "============================================"
	@echo " Environment check"
	@echo "============================================"
	@echo -n "  OS ............... "; uname -sr
	@echo -n "  gcc .............. "
	@if [ -n "$(HAS_GCC)" ]; then \
	    gcc --version | head -1; \
	else \
	    echo "NOT FOUND  --> run: sudo apt-get install -y gcc"; \
	    exit 1; \
	fi
	@echo -n "  make ............. "; make --version | head -1
	@echo -n "  openssl lib ...... "
	@if [ "$(HAS_SSL)" = "yes" ]; then \
	    pkg-config --modversion openssl; \
	else \
	    echo "NOT FOUND  --> run: sudo apt-get install -y libssl-dev"; \
	fi
	@echo -n "  openssl headers .. "
	@if [ "$(HAS_SSLHDR)" = "yes" ]; then \
	    echo "ok  (/usr/include/openssl/evp.h)"; \
	else \
	    echo "NOT FOUND  --> run: sudo apt-get install -y libssl-dev"; \
	    exit 1; \
	fi
	@echo "============================================"
	@echo " All checks passed. Building..."
	@echo "============================================"
