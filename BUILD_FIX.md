# Fix Build Errors - Run These Commands in WSL

## 1. Initialize Git in the boilerplate directory

```bash
cd /mnt/f/rea/ps5-boilerplate
git init
git remote add origin https://github.com/brinza666/movian_ps5.git
git pull origin main --allow-unrelated-histories
```

## 2. Download Duktape JS Engine Source

The build needs `duktape.h` and `duktape.c`. Download Duktape 2.7.0:

```bash
cd /mnt/f/rea/ps5-boilerplate
mkdir -p src/duktape
curl -sL https://github.com/svaarala/duktape/releases/download/2.7.0/duktape-2.7.0.tar.xz | tar xJ -C src/duktape --strip-components=1
```

Then add Duktape to the build. Edit `tools/build.sh` and find the source collection section. After the line:

```bash
mapfile -d '' -t source_paths < <(
    find "$root/src" -type f \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' \) \
        -print0 | sort -z
)
```

Add Duktape source files to the sources array:

```bash
# Add Duktape JS engine
sources+=("src/duktape/duktape.c")
```

Or alternatively, copy duktape.c and duktape.h directly into src/:
```bash
cp src/duktape/duktape.c src/
cp src/duktape/duktape.h src/
```

## 3. Rebuild

```bash
cd /mnt/f/rea/ps5-boilerplate
make
```

## Expected Build Output

If successful, you'll see:
- libc.prx generated (clean-room runtime)
- Source files compiled (main.cpp, duktape_ps5.c, video_ps5.c, etc.)
- Linked into eboot.elf
- Signed into eboot.bin
- App folder created at `dist/PPSA99455/`

## If More Errors Occur

Common issues and fixes:

| Error | Fix |
|-------|-----|
| `duktape.h not found` | Download Duktape (see step 2) |
| `prospero-clang18: not found` | Run `make deps` first to fetch SDK |
| `param.json contentId invalid` | Already fixed — ensure 16-char suffix |
| `FSELF magic error` | Normal — the tool signs with magic 0x1D3D154F |
