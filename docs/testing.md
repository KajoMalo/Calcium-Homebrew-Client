# Running the Test Suite

Calcium Client includes a comprehensive unit test suite built with [Catch2 v3](https://github.com/catchorg/Catch2). All tests run entirely on the desktop — no PS4 hardware, no network connection, and no GUI are required.

---

## Prerequisites

The test suite is enabled by default. Ensure `CALCIUM_BUILD_TESTS=ON` (the default) is set when configuring.

Tests require:
- CMake 3.20+
- A C++17 compiler
- zlib (same requirement as the main build)
- Internet access on first build to fetch Catch2 via FetchContent (or a pre-populated CMake cache)

---

## Building and running all tests

```bash
cd CalciumClient

# Configure with tests enabled.
cmake -B build \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCALCIUM_BUILD_TESTS=ON \
      -DCALCIUM_BUILD_DESKTOP=ON

# Build everything including tests.
cmake --build build --parallel

# Run the full test suite via CTest.
ctest --test-dir build --output-on-failure
```

---

## Running individual test executables

Each test file compiles to its own executable in `build/bin/`:

```bash
# All tests in one file:
./build/bin/test_metadata_parser
./build/bin/test_config
./build/bin/test_installed_database
./build/bin/test_package_verifier
./build/bin/test_download_state
./build/bin/test_repository
./build/bin/test_error_handling
```

---

## Filtering tests

Catch2 supports filtering by test name or tag using `-t` (tags) or a positional pattern:

```bash
# Run only tests tagged [parser]:
./build/bin/test_metadata_parser "[parser]"

# Run only tests whose name contains "sha256":
./build/bin/test_package_verifier "sha256"

# Run only tests tagged [config] across all binaries via CTest:
ctest --test-dir build -R "test_config" --output-on-failure

# List all test cases without running them:
./build/bin/test_repository --list-tests
```

---

## Verbose output

```bash
# Show all PASSED/FAILED lines:
ctest --test-dir build -V

# Or run a single binary with Catch2's verbose reporter:
./build/bin/test_metadata_parser --reporter compact
```

---

## Test coverage by file

| File | Tests | What is covered |
|---|---|---|
| `test_metadata_parser.cpp` | 17 | JSON index parsing, field extraction, validation, compat status, has_update logic, edge cases |
| `test_config.cpp` | 9 | Config load/save, defaults, round-trip serialisation, repo add/remove, malformed JSON |
| `test_installed_database.cpp` | 10 | CRUD operations, upsert/replace, round-trip JSON, disk persistence, error paths |
| `test_package_verifier.cpp` | 11 | SHA-256 correctness (NIST vectors), file verification, hash mismatch, missing file, large file |
| `test_download_state.cpp` | 8 | Download success/failure, cancellation, duplicate enqueue, progress callbacks, DownloadItem helpers |
| `test_repository.cpp` | 17 | Repository refresh (HTTP + file://), field mapping, find_app, RepositoryManager aggregation, search, filtering, callbacks |
| `test_error_handling.cpp` | 12 | Null/wrong-type fields, large indexes, empty files, unwritable paths, corrupt databases, network failures |

**Total: 84 test cases**

---

## Mock objects

### MockHttpClient (`test_download_state.cpp`)
Controls whether downloads succeed or fail, how many failures occur before success, file size written, and how many progress callbacks are emitted.

### RepoMockHttpClient (`test_repository.cpp`)
Returns a configurable JSON body and HTTP status code without any real network I/O.

### AlwaysFailHttpClient (`test_error_handling.cpp`)
Always returns a transport error — used to test repository error-handling paths.

---

## Writing new tests

1. Create a new file `tests/test_<subsystem>.cpp`
2. Add it to the `TEST_SOURCES` list in `tests/CMakeLists.txt`
3. Include the relevant headers from `src/` (the test target has `src/` on its include path)
4. Use standard Catch2 macros: `TEST_CASE`, `REQUIRE`, `CHECK`, `REQUIRE_THROWS`, `REQUIRE_NOTHROW`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "my_subsystem/MyClass.hpp"

TEST_CASE("MyClass - basic behaviour", "[mysubsystem]") {
    MyClass obj;
    obj.do_thing();
    CHECK(obj.result() == expected_value);
}
```

Run `cmake --build build` after adding the file — no further CMake reconfiguration is needed.

---

## Continuous integration

For CI environments without a display (no SDL2 window needed):

```yaml
# Example GitHub Actions step
- name: Configure
  run: |
    cmake -B build \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCALCIUM_BUILD_TESTS=ON \
          -DCALCIUM_BUILD_DESKTOP=OFF

- name: Build
  run: cmake --build build --parallel

- name: Test
  run: ctest --test-dir build --output-on-failure
```

Setting `CALCIUM_BUILD_DESKTOP=OFF` removes the SDL2 dependency entirely, making the build and test run work in a minimal headless environment.

---

## Known limitations

- The `PackageInstaller` ZIP extraction tests require the host to have zlib available (standard on all supported platforms).
- `test_download_state.cpp` spawns real background threads. If a test process is killed mid-run, temporary files in the system temp directory may be left behind — these are harmless and named `calcium_dm_*.bin`.
- The PS4-specific platform code (`PS4Platform.cpp`, `PS4HttpClient.inl`) is excluded from the desktop test build and is only compiled when `CALCIUM_PLATFORM_PS4=ON`.
