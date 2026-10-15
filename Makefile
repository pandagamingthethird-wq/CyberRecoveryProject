CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L
SRC := $(shell find src -name '*.c')
OBJ := $(SRC:.c=.o)
TARGET := cyberflasher

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	find src -name '*.o' -delete
	rm -f $(TARGET)

# NOTE: this builds the host-side logic for inspection/testing only.
# A real on-device recovery binary needs to be cross-compiled against
# the device's toolchain and linked into the recovery ramdisk's init,
# the same way TWRP's own sources are — see docs/ARCHITECTURE.md.
