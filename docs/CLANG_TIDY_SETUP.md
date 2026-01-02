# Clang-Tidy Setup for fast-containers

## Current Status

✅ **Configuration created**: `.clang-tidy` with STL-style naming conventions
✅ **Initial suppressions applied**: Union access, magic numbers, etc.
📊 **Remaining warnings**: 54 (down from 200+)

## Configuration Summary

### Enabled Checks
- `bugprone-*` - Catch common bugs
- `clang-analyzer-*` - Static analysis
- `cppcoreguidelines-*` - C++ Core Guidelines
- `google-*` - Google C++ Style Guide
- `modernize-*` - Modern C++ features
- `performance-*` - Performance improvements
- `readability-*` - Code readability

### Suppressed Checks (Intentional)
- `cppcoreguidelines-pro-type-union-access` - Btree uses union for root node
- `google-runtime-int` - Template metaprogramming needs built-in types
- `cppcoreguidelines-avoid-magic-numbers` - Bit manipulation constants
- `readability-magic-numbers` - Same as above
- `readability-identifier-length` - Short names like `i`, `it` are fine
- `performance-enum-size` - Not worth optimizing

### Naming Conventions (STL-style)
```cpp
class dense_map { };                    // ✓ snake_case (not DenseMap)
void insert_hint() { }                  // ✓ snake_case
const auto idx = 5;                     // ✓ lower_case
enum class SearchMode { Binary, SIMD }; // ✓ CamelCase enum + values
template <typename Key, typename Value> // ✓ CamelCase templates
```

## Remaining Issues (54 warnings)

### 1. Missing [[nodiscard]] (7 warnings) - **EASY FIX**
**Time**: ~5 minutes

Add `[[nodiscard]]` to query functions that return values:

```cpp
// Before:
size_type size() const { return size_; }
bool empty() const { return size_ == 0; }

// After:
[[nodiscard]] size_type size() const { return size_; }
[[nodiscard]] bool empty() const { return size_ == 0; }
```

**Files**: `dense_map.hpp` (4), `btree.hpp` (2), `dense_map_iterator` (1)

### 2. else-after-return (9 warnings) - **EASY FIX**
**Time**: ~10 minutes

Remove redundant else after return:

```cpp
// Before:
if (condition) {
  return value;
} else {
  return other;
}

// After:
if (condition) {
  return value;
}
return other;
```

**Files**: `btree.ipp` (9 occurrences)

### 3. Uninitialized Variables (8 warnings) - **EASY FIX**
**Time**: ~5 minutes

Initialize variables at declaration (need to find these via full scan)

### 4. Uppercase Literal Suffixes (4 warnings) - **EASY FIX**
**Time**: ~2 minutes

Change lowercase to uppercase suffixes:

```cpp
// Before:
constexpr size_t val = 1ull;

// After:
constexpr size_t val = 1ULL;
```

### 5. C-style Casts (4 warnings) - **MODERATE FIX**
**Time**: ~5 minutes

**Location**: `btree.hpp` lines 44, 78 (byte size calculations)

```cpp
// Before:
(size_t)(LeafNodeSize * sizeof(std::pair<Key, Value>))

// After:
static_cast<size_t>(LeafNodeSize * sizeof(std::pair<Key, Value>))
```

### 6. Member Initializers (4 warnings) - **MODERATE FIX**
**Time**: ~5 minutes

**Location**: `btree.ipp` constructor (leftmost_leaf_, rightmost_leaf_)

```cpp
// Before:
btree() : root_is_leaf_(true), leaf_root_(new LeafNode()), size_(0) {
  leftmost_leaf_ = leaf_root_;
  rightmost_leaf_ = leaf_root_;
}

// After:
btree()
  : root_is_leaf_(true),
    leaf_root_(new LeafNode()),
    size_(0),
    leftmost_leaf_(leaf_root_),
    rightmost_leaf_(leaf_root_) {}
```

### 7. Missing std::forward (4 warnings) - **MODERATE FIX**
**Time**: ~10 minutes

**Location**: `btree.ipp` lines 844, 916 (lambda parameters)

```cpp
// Before:
template <typename GetValue>
void func(GetValue&& get_value) {
  auto val = get_value();  // Warning: not forwarded
}

// After:
template <typename GetValue>
void func(GetValue&& get_value) {
  auto val = std::forward<GetValue>(get_value)();
}
```

### 8. Explicit Constructors (3 warnings) - **REVIEW**
**Time**: ~5 minutes

Mark single-argument constructors explicit to prevent implicit conversions.

**Note**: Check if implicit conversion is intentional before fixing.

### 9. Special Member Functions (2 warnings) - **REVIEW**
**Time**: ~15 minutes

**Issue**: `InternalNode` defines copy but not move

**Options**:
1. Add move constructor/assignment
2. Delete copy/move if not needed
3. Use `= default` for move if trivial

**Recommendation**: Add move operations for performance:
```cpp
InternalNode(InternalNode&&) noexcept = default;
InternalNode& operator=(InternalNode&&) noexcept = default;
```

### 10. Naming Issues (2 warnings) - **FALSE POSITIVE**
**Location**: Forward declarations in `btree.hpp`

```cpp
struct InternalNode;  // Warning: wants internal_node
struct LeafNode;      // Warning: wants leaf_node
```

**Action**: Ignore - actual definitions are correctly named

### 11. bugprone-return-const-ref-from-parameter (1 warning) - **REVIEW**
**Location**: `btree.ipp:403`

**Issue**: Returning `const Key&` that may be a temporary

**Action**: Review code, likely need to return by value or store differently

## How to Run

### Check specific files:
```bash
clang-tidy-19 -p cmake-build-clang-tidy include/fast_containers/dense_map.hpp
```

### Check all files:
```bash
run-clang-tidy-19 -p cmake-build-clang-tidy
```

### Count warnings by type:
```bash
clang-tidy-19 -p cmake-build-clang-tidy include/fast_containers/*.hpp 2>&1 | \
  grep "warning:" | sed 's/.*\[\(.*\)\]/\1/' | sort | uniq -c | sort -rn
```

## Recommended Fix Order

### Phase 1: Quick Wins (~30 minutes)
1. Add [[nodiscard]] (7 fixes)
2. Remove else-after-return (9 fixes)
3. Uppercase literal suffixes (4 fixes)
4. C-style casts (4 fixes)

**Impact**: Fixes 24/54 warnings (44%)

### Phase 2: Moderate Effort (~30 minutes)
1. Uninitialized variables (8 fixes)
2. Member initializers (4 fixes)
3. Add std::forward (4 fixes)

**Impact**: Fixes 16/54 warnings (30%)

### Phase 3: Review Required (~30 minutes)
1. Special member functions (2 issues)
2. Explicit constructors (3 issues)
3. Return const ref issue (1 issue)

**Impact**: Fixes 6/54 warnings (11%)

**Total effort**: ~1.5 hours to resolve all actionable warnings

## Enabling clang-tidy in CMake

### Option 1: Global (all targets)
Add to `CMakeLists.txt` after `project()`:

```cmake
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy-19 clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY
            ${CLANG_TIDY_EXE};
            -config-file=${CMAKE_SOURCE_DIR}/.clang-tidy;
        )
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "clang-tidy not found")
    endif()
endif()
```

### Option 2: Target-specific (recommended)
Apply only to test targets:

```cmake
if(CLANG_TIDY_EXE)
    set_target_properties(test_dense_map test_btree PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
    )
endif()
```

### Option 3: Manual only (safest initially)
Don't enable in CMake, run manually when needed:

```bash
# Run before committing
clang-tidy-19 -p build <changed-files>
```

## Next Steps

1. **Review this report** - Decide which warnings to fix
2. **Fix Phase 1 issues** - Quick wins, low risk
3. **Test thoroughly** - Ensure fixes don't break tests
4. **Enable in CI** - Add to GitHub Actions/pre-commit hook
5. **Expand to other files** - Apply to allocator headers

## Files Created

- `.clang-tidy` - Configuration file
- `clang-tidy-report.md` - Detailed analysis
- `CLANG_TIDY_SETUP.md` - This file
- `cmake-build-clang-tidy/compile_commands.json` - For IDE integration

## IDE Integration

Most IDEs can use compile_commands.json automatically:

- **VSCode**: Install "clangd" extension
- **CLion**: Settings → Build → CMake → Enable clang-tidy
- **Vim/Neovim**: Use ALE or coc-clangd

Point to: `/mnt/flash0/home/kressler/git/fast-containers/cmake-build-clang-tidy/compile_commands.json`
