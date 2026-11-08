# Movian PS5 Port - Contributing Guidelines

## Getting Started

1. Fork the repository
2. Clone your fork
3. Create a feature branch: `git checkout -b feature/your-feature`
4. Make your changes
5. Test on PS5 hardware (firmware 4.03)
6. Submit a pull request

## Build Environment

- WSL2 Ubuntu or native Linux
- clang-18, lld-18, make, python3
- PS5 firmware 4.03 with homebrew support
- ShadowMountPlus or similar loader
- etaHEN on port 9021 (for FTP deploy)

## Code Style

- C files: Follow the existing style (4-space indent, K&R braces)
- C++20 files: Follow boilerplate conventions
- Commit messages: Use conventional commits (feat:, fix:, docs:, etc.)

## Testing

- Build with `make doctor` to verify environment
- Build with `make` to compile
- Test on PS5 hardware
- Report bugs with firmware version and error details

## License

GPL-3.0-or-later
