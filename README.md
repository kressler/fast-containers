# Fast Containers

![CI](https://github.com/kressler/fast-containers/workflows/CI/badge.svg)

High-performance header-only container library for C++ on x86-64

## Tech Stack

- **Language**: C++23
- **Build System**: CMake 3.30+
- **Testing**: Catch2 v3.11.0
- **Code Formatting**: clang-format (Google C++ Style)

## Building

```bash
# Configure
cmake -S . -B build

# Build
cmake --build build

# Run tests
ctest --test-dir build
```

## Code Formatting

This project uses clang-format with Google C++ Style Guide conventions.

### Format All Files

```bash
cmake --build build --target format
```

### Pre-commit Hook (Automatic Formatting)

To automatically format C++ files before each commit, install the pre-commit hook:

```bash
./install-hooks.sh
```

This will:
- Automatically format all staged C++ files (.cpp, .hpp) with clang-format
- Re-stage formatted files
- Continue with the commit

**Requirements**: clang-format must be installed on your system

**Bypass hook** (when needed):
```bash
git commit --no-verify
```

## Project Structure

```
.
├── include/
│   └── fast_containers/   # Public header files
│       ├── btree.hpp, btree.ipp
│       ├── ordered_array.hpp, ordered_array.ipp
│       └── hugepage_*.hpp
├── tests/                 # Unit tests (Catch2)
├── third_party/           # Git submodules (Catch2)
├── hooks/                 # Git hooks (install with install-hooks.sh)
└── CMakeLists.txt         # Build configuration
```

## Development Workflow

1. Clone the repository with submodules:
   ```bash
   git clone --recursive https://github.com/kressler/fast-containers.git
   ```
2. Install the pre-commit hook: `./install-hooks.sh`
3. Make your changes
4. Build and test: `cmake --build build && ctest --test-dir build`
5. Commit (files will be auto-formatted)

## Code Conventions

- Follow Google C++ Style Guide (enforced by clang-format)
- Use C++23 features
- Write tests for new functionality using Catch2
- Run `cmake --build build --target format` before submitting PRs

## License

[Add license information here]
