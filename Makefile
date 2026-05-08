# micro-ilink Makefile
# Simple, explicit rules suitable for embedded Linux / cross-compilation.

# ── Toolchain (override for cross-compile) ─────────────────────────────
CC      ?= gcc
AR      ?= ar
RM      ?= rm -f
MKDIR   ?= mkdir -p

# ── Flags ──────────────────────────────────────────────────────────────
INCLUDES  = -I./include
WFLAGS    = -Wall -Wextra

# Release (default)
OPT       = -O2
DBG       =

# Libraries
LDLIBS    = -lcurl -lssl -lcrypto -lm

# ── Source files ───────────────────────────────────────────────────────
SRCS  = src/utils.c   \
        src/crypto.c  \
        src/http.c    \
        src/config.c  \
        src/ilink.c   \
        src/cdn.c     \
        src/bot.c

OBJS  = $(SRCS:src/%.c=build/%.o)
DEPS  = $(OBJS:%.o=%.d)

LIB   = build/libilink.a

# ── Example programs ───────────────────────────────────────────────────
EXAMPLES = build/bin/echo_bot        \
           build/bin/test_image_send \
           build/bin/test_video_send \
           build/bin/test_file_send

# ── Targets ────────────────────────────────────────────────────────────
.PHONY: all release debug clean

all: release

release: OPT = -O2
release: DBG =
release: CFLAGS = $(WFLAGS) $(INCLUDES) $(OPT) $(DBG)
release: $(LIB) $(EXAMPLES)
	@echo "  Release build complete."

debug: OPT = -O0
debug: DBG = -g -DILINK_DEBUG
debug: CFLAGS = $(WFLAGS) $(INCLUDES) $(OPT) $(DBG)
debug: clean $(LIB) $(EXAMPLES)
	@echo "  Debug build complete."

# ── Static library ─────────────────────────────────────────────────────
$(LIB): $(OBJS)
	@echo "  AR      $@"
	@$(AR) rcs $@ $(OBJS)

# ── Object files ───────────────────────────────────────────────────────
build/%.o: src/%.c | build
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build build/bin:
	@$(MKDIR) $@

# ── Example binaries ───────────────────────────────────────────────────
build/bin/echo_bot: examples/echo_bot.c $(LIB) | build/bin
	@echo "  LINK    $@"
	@$(CC) $(CFLAGS) $< $(LIB) -o $@ $(LDLIBS)

build/bin/test_image_send: examples/test_image_send.c $(LIB) | build/bin
	@echo "  LINK    $@"
	@$(CC) $(CFLAGS) $< $(LIB) -o $@ $(LDLIBS)

build/bin/test_video_send: examples/test_video_send.c $(LIB) | build/bin
	@echo "  LINK    $@"
	@$(CC) $(CFLAGS) $< $(LIB) -o $@ $(LDLIBS)

build/bin/test_file_send: examples/test_file_send.c $(LIB) | build/bin
	@echo "  LINK    $@"
	@$(CC) $(CFLAGS) $< $(LIB) -o $@ $(LDLIBS)

# ── Clean ──────────────────────────────────────────────────────────────
clean:
	@echo "  CLEAN"
	@$(RM) -r build

# ── Dependencies ───────────────────────────────────────────────────────
-include $(DEPS)
