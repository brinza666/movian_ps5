# Drop-in: apply.sh appends `-include ps5.mk` to the movian-go Makefile.
# Builds an Orbis ELF *shape*. It runs only with the patched Go toolchain
# described in runtime/README.md.

PS5_PAYLOAD_SDK ?= /opt/ps5-payload-sdk
GO_PS5 ?= go

PS5_CC  := $(PS5_PAYLOAD_SDK)/bin/prospero-clang
PS5_CXX := $(PS5_PAYLOAD_SDK)/bin/prospero-clang++
PS5_PKG := $(PS5_PAYLOAD_SDK)/bin/prospero-pkg-config

.PHONY: ps5 ps5-ffmpeg ps5-sdl

ps5:
	@test -x $(PS5_CC) || { echo "prospero-clang not at $(PS5_CC). Install ps5-payload-sdk 0.45+."; exit 1; }
	@test -f ffmpeg/ps5/lib/libavcodec.a || $(MAKE) ps5-ffmpeg
	PKG_CONFIG=$(PS5_PKG) \
	CC=$(PS5_CC) CXX=$(PS5_CXX) \
	CGO_ENABLED=1 GOOS=freebsd GOARCH=amd64 \
	$(GO_PS5) build -trimpath -tags ps5 \
		-ldflags "$(LDFLAGS) -linkmode=external" \
		-o movian-go.elf ./cmd/movian-go

ps5-sdl:
	./scripts/build_sdl_ps5.sh

ps5-ffmpeg:
	./scripts/build_ffmpeg_ps5.sh
