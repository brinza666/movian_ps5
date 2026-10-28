# SPLICER — native PS5 payload. Link with ps5-payload-sdk (prospero-clang).

PS5_PAYLOAD_SDK ?= /tmp/ps5sdk/ps5-payload-sdk
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

ELF := payload.elf

CFLAGS := -O2 -Ithird_party -Wall -Wno-unused-function -Wno-unused-variable
LIBS := -lSceVideoOut -lSceSystemService

all: $(ELF)

$(ELF): src/main.c src/mpeg_dec.c third_party/pl_mpeg.h third_party/font8x8_basic.h
	$(CC) $(CFLAGS) -o $@ src/main.c src/mpeg_dec.c $(LIBS)

clean:
	rm -f $(ELF)
