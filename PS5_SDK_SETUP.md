# PS5 SDK Setup Instructions

The PS5 SDK is proprietary Sony software. You need official PlayStation developer access to download it.

## Obtain the PS5 SDK

1. **Register as a PlayStation Developer**: https://partners.playstation.net/
2. **Download PS5 SDK** from the Sony developer portal
3. **Install** to a location like `C:\ps5-sdk` or `F:\ps5-sdk`

## Required SDK Tools

| Tool | Purpose |
|------|---------|
| `prospero-clang` | PS5 ARM64 cross-compiler |
| `prospero-clang++` | PS5 ARM64 cross-compiler (C++) |
| `prospero-ar` | PS5 archiver |
| `prospero-strip` | PS5 symbol stripper |
| `ps5-make-fself` | Convert ELF to FSELF format |
| `ps5-make-fpkg` | Create fake package (.fpkg) |
| `ps5-sfo-tool` | Create param.sfo files |

## Environment Variables

Set these in your system or user environment:

```powershell
[System.Environment]::SetEnvironmentVariable("PS5_SDK_DIR", "C:\ps5-sdk", [System.EnvironmentVariableTarget]::User)
[System.Environment]::SetEnvironmentVariable("PS5_CLANG", "prospero-clang", [System.EnvironmentVariableTarget]::User)
[System.Environment]::SetEnvironmentVariable("PS5_AR", "prospero-ar", [System.EnvironmentVariableTarget]::User)
[System.Environment]::SetEnvironmentVariable("PS5_STRIP", "prospero-strip", [System.EnvironmentVariableTarget]::User)
[System.Environment]::SetEnvironmentVariable("PS5_FSELF", "ps5-make-fself", [System.EnvironmentVariableTarget]::User)
[System.Environment]::SetEnvironmentVariable("PS5_FPKG", "ps5-make-fpkg", [System.EnvironmentVariableTarget]::User)
```

Or set them per-session in PowerShell:
```powershell
$env:PS5_SDK_DIR = "C:\ps5-sdk"
$env:PS5_CLANG = "prospero-clang"
# etc.
```

## Verify SDK Installation

```powershell
prospero-clang --version
ps5-make-fself --help
ps5-make-fpkg --help
```
