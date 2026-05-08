# tools/ Makefile — standalone, independent from micro-ilink main build.
#
#   make                build login
#   make QRENCODE=1     build login with terminal QR display (needs libqrencode-dev)
#   make clean          remove built binary
#
# Requires: libcurl-dev
# Optional: libqrencode-dev (sudo apt install libqrencode-dev)

CC      ?= gcc
RM      ?= rm -f

CFLAGS   = -Wall -Wextra -O2
LDLIBS   = -lcurl

TARGET   = login

# ── Optional: terminal QR display ────────────────────────────────────
ifdef QRENCODE
CFLAGS  += -DHAS_QRENCODE
LDLIBS  += -lqrencode
endif

# ── Targets ───────────────────────────────────────────────────────────
.PHONY: all clean

all: $(TARGET)

$(TARGET): login.c
	@echo "  CC      tools/$@"
	@$(CC) $(CFLAGS) login.c -o $@ $(LDLIBS)

clean:
	@echo "  CLEAN   tools/"
	@$(RM) $(TARGET)
