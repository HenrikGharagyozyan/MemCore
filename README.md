[![CI](https://github.com/HenrikGharagyozyan/MemCore/actions/workflows/cmake.yml/badge.svg)](https://github.com/HenrikGharagyozyan/MemCore/actions/workflows/cmake.yml)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/compiler_support/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

# 🚀 MemCore

**MemCore** is a **C++20 library of composable, concept-checked memory allocators** — bump, stack, pool, arena — plus debugging and profiling decorators, aimed at performance-sensitive code such as game engines, real-time systems, and simulations.

Instead of scattering `new`/`delete` and `malloc`/`free`, MemCore lets you assemble an allocation strategy with predictable cost and explicit lifetime control, and compose it from small, testable pieces.

> **Status: `0.1.0`, pre-1.0 and evolving.** MemCore is built primarily as a deep study of memory management, so its coverage grows deliberately (a coalescing free-list, buddy allocator, and concurrency work are still on the roadmap). The pieces that exist are tested — including under AddressSanitizer/UBSan — but the API is not yet stable. See the roadmap below.

---

## ✨ Highlights

- **Concept-driven contract.** Every allocator satisfies the `Allocator` concept (`allocate` / `deallocate`); capabilities that not all allocators can provide are modeled as refinements — `ResettableAllocator` (`reset()`) and `OwningAllocator` (`owns()`). Compositions are constrained on these concepts, so misuse is a compile error with a readable message rather than a runtime surprise.
- **Composable by design.** Wrap any allocator with decorators — bounds-checking canaries, tagged tracking, a mutex, or a fallback path — without touching the underlying allocator.
- **Alignment-correct.** Over-aligned types (e.g. `alignas(64)`) are handled throughout, and the whole suite runs clean under ASan/UBSan in CI.
- **STL & language integration.** `StlAdapter` plugs any allocator into standard containers; `New`/`Delete` helpers and an `operator new`/`delete` injection macro cover object lifetimes.

## 🧱 Architecture

```
        Adapters:   StlAdapter · New/Delete · MEMCORE_ENABLE_CLASS_ALLOCATOR
                                     |
        Decorators: Tracker · Canary · ThreadSafe · Fallback   (wrap an Allocator)
                                     |
        Allocators: Linear · Stack · Pool · Arena              (own a region)
                                     |
        Upstream:   MallocUpstream · VirtualUpstream           (raw memory from the OS)
```

## 📦 Components

### Upstream sources
| Type | Backed by | Notes |
|---|---|---|
| `MallocUpstream` | `posix_memalign` / `_aligned_malloc` | Portable, alignment-correct. Non-owning (see `OwningAllocator`). |
| `VirtualUpstream` | `mmap` / `VirtualAlloc` | Page-granular OS memory. |

### Allocators
| Allocator | Allocation | Deallocation | Use case |
|---|---|---|---|
| `LinearAllocator` | O(1) | reset only | Frame / scratch memory |
| `StackAllocator` | O(1) | O(1) LIFO (+ markers) | Nested scoped allocations |
| `PoolAllocator` | O(1) | O(1), any order | Many same-size objects |
| `ArenaAllocator` | O(1) amortized | reset only | Growing collections |

### Decorators (wrap any `Allocator`)
| Decorator | Adds |
|---|---|
| `TrackerAllocator` | Per-tag usage & peak stats, leak reporting |
| `CanaryAllocator` | Front/back guard bytes; aborts on over/underflow |
| `ThreadSafeAllocator` | Mutex around a wrapped allocator |
| `FallbackAllocator` | Primary allocator with a fallback on failure |

## 📚 Requirements

- A C++20 compiler: GCC 11+, Clang 14+, or MSVC 2022+
- CMake 3.21+

CI builds and tests on Ubuntu, Windows, and macOS in Debug and Release, plus a dedicated ASan+UBSan run.

## 🔧 Build & test

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Useful options:

| Option | Default | Effect |
|---|---|---|
| `MEMCORE_BUILD_TESTS` | `ON` when top-level | Build the unit tests (fetches googletest) |
| `MEMCORE_INSTALL` | `ON` when top-level | Generate install & `find_package` rules |

Run the suite under sanitizers:

```bash
cmake -B build-san -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all"
cmake --build build-san
ctest --test-dir build-san --output-on-failure
```

## 📥 Using MemCore in your project

### Installed package

```bash
cmake -B build -DMEMCORE_BUILD_TESTS=OFF
cmake --build build
cmake --install build --prefix /your/prefix
```

```cmake
find_package(MemCore 0.1 REQUIRED)
target_link_libraries(my_app PRIVATE MemCore::MemCore)
```

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    MemCore
    GIT_REPOSITORY https://github.com/HenrikGharagyozyan/MemCore.git
    GIT_TAG main
)
FetchContent_MakeAvailable(MemCore)   # tests are off automatically as a subproject

target_link_libraries(my_app PRIVATE MemCore::MemCore)
```

## ⚡ Quick start

### Linear (bump) allocator

```cpp
#include <MemCore/MallocUpstream.hpp>
#include <MemCore/LinearAllocator.hpp>

MemCore::MallocUpstream upstream;
MemCore::Block chunk = upstream.allocate(1024, alignof(std::max_align_t));

MemCore::LinearAllocator arena(chunk);
MemCore::Block b = arena.allocate(sizeof(int), alignof(int));
*static_cast<int*>(b.ptr) = 42;

arena.reset();                                 // reclaim everything at once
upstream.deallocate(chunk.ptr, chunk.size);
```

### STL container on a custom allocator

```cpp
#include <vector>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/StlAdapter.hpp>

MemCore::LinearAllocator arena(chunk);
using Alloc = MemCore::StlAdapter<int, MemCore::LinearAllocator>;

std::vector<int, Alloc> data{ Alloc(arena) };
data.push_back(10);
```

### Tagged tracking

```cpp
#include <MemCore/TrackerAllocator.hpp>
#include <MemCore/MemoryTags.hpp>

MemCore::TrackerAllocator tracker(upstream);   // wraps any allocator
{
    MemCore::TagScope gfx(MemCore::MemoryTag::Graphics);
    MemCore::Block b = tracker.allocate(256, 8);
    // ... use b ...
    tracker.deallocate(b.ptr, b.size);
}
tracker.print_stats();                         // per-tag current & peak, leak check
```

## 🗺️ Roadmap

Implemented: aligned upstreams, linear/stack/pool/arena allocators, STL & class adapters, tagged tracking, canary debugging, fallback and thread-safe decorators, a concept-based contract, install/packaging, and CI with sanitizers.

Planned: coalescing free-list allocator, buddy allocator, benchmarks vs `malloc`/`std::pmr`, and a dedicated concurrency layer.

## 📜 License

MemCore is licensed under the MIT License. See [LICENSE](LICENSE).
