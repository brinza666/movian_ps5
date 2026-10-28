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
OBJS := main.o mpeg_dec.o avdec.o hwdec.o duktape.o plugin_host.o bt.o
DUK_SRC := /tmp/duktape/duktape-2.7.0/src/duktape.c

all: $(ELF)

main.o: src/main.c src/avdec.h third_party/pl_mpeg.h
	gcc $(CFLAGS) -c -o $@ src/main.c
mpeg_dec.o: src/mpeg_dec.c
	gcc $(CFLAGS) -c -o $@ src/mpeg_dec.c
avdec.o: src/avdec.c src/avdec.h src/hwdec.h
	gcc $(CFLAGS) -c -o $@ src/avdec.c
hwdec.o: src/hwdec.c src/hwdec.h
	gcc $(CFLAGS) -c -o $@ src/hwdec.c
src/plugin_embed.h: src/plugin_boot.js tools/embed_js.py $(wildcard js/ecmascript/*/*.js) $(wildcard js/ecmascript/*/*/*.js)
	python3 tools/embed_js.py
plugin_host.o: src/plugin_host.c src/plugin_host.h src/plugin_embed.h
	gcc $(CFLAGS) -I$(dir $(DUK_SRC)) -c -o $@ src/plugin_host.c
bt.o: src/bt.c src/bt.h
	gcc $(CFLAGS) -c -o $@ src/bt.c
duktape.o: $(DUK_SRC) third_party/endian.h
	gcc $(CFLAGS) -c -o $@ $(DUK_SRC)

$(ELF): $(OBJS)
	ld -pie -T $(PS5_PAYLOAD_SDK)/target/lib/main.script --eh-frame-hdr --build-id=none \
		-o $@ $(PS5_PAYLOAD_SDK)/target/lib/crt1.o $(OBJS) \
		-L $(PS5_PAYLOAD_SDK)/target/lib \
		--start-group \
		$(FFMPEG_SRC)/libavformat/libavformat.a \
		$(FFMPEG_SRC)/libavcodec/libavcodec.a \
		$(FFMPEG_SRC)/libswresample/libswresample.a \
		$(FFMPEG_SRC)/libavutil/libavutil.a \
		/tmp/zlib-1.3.1/libz.a \
		--end-group \
		-lSceVideoOut -lSceSystemService -lkernel_web -lSceLibcInternal -lSceNet \
		-lSceSsl -lSceHttp2 -lScePosixForWebKit

clean:
	rm -f $(ELF) $(OBJS)
