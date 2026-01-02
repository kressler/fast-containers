# Fast Containers

![CI](https://github.com/kressler/fast-containers/workflows/CI/badge.svg)

High-performance header-only container library for C++ on x86-64

## Tech Stack

- **Language**: C++23
- **Build System**: CMake 3.30+
- **Testing**: Catch2 v3.11.0
- **Code Formatting**: clang-format (Google C++ Style)

## Building

### Using CMake Presets (Recommended)

```bash
# List available presets
cmake --list-presets

# Configure, build, and test in one workflow
cmake --preset release
cmake --build --preset release
ctest --preset release

# Common presets:
cmake --preset debug          # Debug build
cmake --preset release        # Release with AVX2 (default)
cmake --preset asan           # AddressSanitizer build
cmake --preset release-no-avx2  # Release without AVX2
```

### Manual Build (Alternative)

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_AVX2` | `ON` (Release), `OFF` (Debug) | Enable AVX2 SIMD optimizations |
| `ENABLE_ASAN` | `OFF` | Enable AddressSanitizer |
| `ENABLE_ALLOCATOR_STATS` | `OFF` | Enable allocator statistics |
| `ENABLE_LTO` | `ON` | Enable Link-Time Optimization |
| `ENABLE_NUMA` | Auto-detected | Enable NUMA support (requires libnuma) |

## Code Quality

This project uses clang-format for code formatting and clang-tidy for static analysis.

### Code Formatting

Format all files:
```bash
cmake --build build --target format
```

### Static Analysis

Run clang-tidy on all production headers:
```bash
cmake --build build --target clang-tidy
```

Or run manually:
```bash
clang-tidy-19 -p cmake-build-clang-tidy include/fast_containers/*.hpp
```

**Note**: Requires `cmake-build-clang-tidy/compile_commands.json` (created by `setup-dev.sh`)

### Pre-commit Hook (Automatic Formatting & Checks)

To automatically format and check C++ files before each commit:

```bash
./setup-dev.sh  # One-time setup
```

The pre-commit hook will:
- Automatically format all staged C++ files with clang-format
- Check production headers with clang-tidy (production code only)
- Fail the commit if clang-tidy finds warnings
- Re-stage formatted files
- Auto-create `cmake-build-clang-tidy/` if missing

**Requirements**:
- clang-format (for formatting)
- clang-tidy-19 (for static analysis)
- cmake (to auto-create cmake-build-clang-tidy/ when needed)

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
├── hooks/                 # Git hooks (install with setup-dev.sh)
└── CMakeLists.txt         # Build configuration
```

## Development Workflow

1. Clone the repository with submodules:
   ```bash
   git clone --recursive https://github.com/kressler/fast-containers.git
   cd fast-containers
   ```

2. Set up development environment (one-time):
   ```bash
   ./setup-dev.sh
   ```
   This installs pre-commit hooks and configures clang-tidy.

3. Make your changes

4. Build and test:
   ```bash
   cmake --build build && ctest --test-dir build
   ```

5. Commit (files will be auto-formatted and checked):
   ```bash
   git add .
   git commit -m "Your changes"
   # Pre-commit hook runs automatically:
   #  ✅ Formats code with clang-format
   #  ✅ Checks production headers with clang-tidy
   ```

## Code Conventions

- Follow Google C++ Style Guide (enforced by clang-format)
- Use C++23 features
- Write tests for new functionality using Catch2
- Production code must be clang-tidy clean (enforced in CI and pre-commit)
- Run `cmake --build build --target format` before submitting PRs

## License

[Add license information here]
