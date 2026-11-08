# 4.03 test payloads. The ELFs in out/ are already linked.
# Rebuild only on a machine that has ps5-payload-sdk:
#   export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
SDK ?= $(PS5_PAYLOAD_SDK)
ifeq ($(SDK),)
SDK := /opt/ps5-payload-sdk
endif
CC ?= gcc
OUT := out

CFLAGS := -O2 -fPIC -fno-stack-protector -fno-plt -Wall -Wno-unused-function \
	-nostdinc -isystem $(SDK)/target/include -Isrc -I../third_party

.PHONY: all clean
all: $(OUT)/splicer-linear.elf $(OUT)/splicer-mpeg1.elf

$(OUT)/linear.o: src/linear.c
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -c -o $@ src/linear.c

$(OUT)/mpeg1.o: src/mpeg1.c ../third_party/pl_mpeg.h
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -c -o $@ src/mpeg1.c

$(OUT)/splicer-linear.elf: $(OUT)/linear.o
	ld -pie -T $(SDK)/target/lib/main.script --eh-frame-hdr --build-id=none \
		-o $@ $(SDK)/target/lib/crt1.o $< \
		-L $(SDK)/target/lib \
		-lc -lSceVideoOut -lSceSystemService -lkernel_web -lSceLibcInternal

$(OUT)/splicer-mpeg1.elf: $(OUT)/mpeg1.o
	ld -pie -T $(SDK)/target/lib/main.script --eh-frame-hdr --build-id=none \
		-o $@ $(SDK)/target/lib/crt1.o $< \
		-L $(SDK)/target/lib \
		-lc -lSceVideoOut -lSceSystemService -lkernel_web -lSceLibcInternal

clean:
	rm -f $(OUT)/*.o
