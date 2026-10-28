# SPLICER payload.
# prospero-clang is the SDK's compiler. This tree was linked with host gcc
# because that wrapper had no host LLVM. The crt and import stubs are still
# the ps5-payload-sdk ones. Entry is _start, loader port 9021.

PS5_PAYLOAD_SDK ?= /tmp/ps5sdk/ps5-payload-sdk
FFMPEG_SRC ?= /tmp/ffsrc/ffmpeg-7.1

ELF := payload.elf
CFLAGS := -O2 -fPIC -fno-stack-protector -fno-plt -Wall -Wno-unused-function -Wno-unused-variable \
	-nostdinc -isystem $(PS5_PAYLOAD_SDK)/target/include \
	-Ithird_party -I$(FFMPEG_SRC)
OBJS := main.o mpeg_dec.o avdec.o

all: $(ELF)

main.o: src/main.c src/avdec.h third_party/pl_mpeg.h
	gcc $(CFLAGS) -c -o $@ src/main.c
mpeg_dec.o: src/mpeg_dec.c
	gcc $(CFLAGS) -c -o $@ src/mpeg_dec.c
avdec.o: src/avdec.c src/avdec.h
	gcc $(CFLAGS) -c -o $@ src/avdec.c

$(ELF): $(OBJS)
	ld -pie -T $(PS5_PAYLOAD_SDK)/target/lib/main.script --eh-frame-hdr --build-id=none \
		-o $@ $(PS5_PAYLOAD_SDK)/target/lib/crt1.o $(OBJS) \
		-L $(PS5_PAYLOAD_SDK)/target/lib \
		--start-group \
		$(FFMPEG_SRC)/libavformat/libavformat.a \
		$(FFMPEG_SRC)/libavcodec/libavcodec.a \
		$(FFMPEG_SRC)/libavutil/libavutil.a \
		--end-group \
		-lSceVideoOut -lSceSystemService -lkernel_web -lSceLibcInternal -lSceNet

clean:
	rm -f $(ELF) $(OBJS)
