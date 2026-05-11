CC=gcc
CLIB_DIR=deps/clib
CLIB_LIB=$(CLIB_DIR)/libclib.a

CFLAGS=-Wall -Wextra -pedantic -std=c11 -Iinclude -I$(CLIB_DIR)/include -MMD -MP
LDFLAGS=-L$(CLIB_DIR) -lclib
DEVFLAGS=-fsanitize=address,undefined -g
RELFLAGS=-O3

BIN=chttp
SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)

TESTSRC = $(wildcard tests/test_*.c)
TESTBIN = $(TESTSRC:.c=)

DEPS = $(OBJ:.o=.d) $(TESTSRC:.c=.d)

.PHONY: all clean test debug setup_deps

all: $(BIN)

debug: CFLAGS += $(DEVFLAGS)
debug: LDFLAGS += $(DEVFLAGS)
debug: all

$(BIN): $(OBJ) $(CLIB_LIB)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $@

$(CLIB_LIB):
	$(MAKE) -C $(CLIB_DIR)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

setup_deps:
	git submodule update --init --recursive

test: CFLAGS += $(DEVFLAGS)
test: LDFLAGS += $(DEVFLAGS)
test: $(TESTBIN)
	@for bin in $(TESTBIN); do ./$$bin; done

tests/%: tests/%.c $(CLIB_LIB)
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

-include $(DEPS)

clean:
	rm -f src/*.o $(BIN) $(TESTBIN) $(DEPS)
	$(MAKE) -C $(CLIB_DIR) clean
