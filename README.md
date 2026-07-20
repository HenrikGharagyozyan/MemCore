[![CI](https://github.com/HenrikGharagyozyan/MemCore/actions/workflows/cmake.yml/badge.svg)](https://github.com/HenrikGharagyozyan/MemCore/actions/workflows/cmake.yml)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/compiler_support/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

# 🚀 MemCore

**MemCore** is a **C++20 library of composable, concept-checked memory allocators** — bump, stack, pool, arena — plus debugging and profiling decorators, aimed at performance-sensitive code such as game engines, real-time systems, and simulations.

Instead of scattering `new`/`delete` and `malloc`/`free`, MemCore lets you assemble an allocation strategy with predictable cost and explicit lifetime control, and compose it from small, testable pieces.

> **Status: `1.0.0` — the API is stable.** Everything documented here follows [semantic versioning](https://semver.org): no breaking changes to the public API within `1.x`. New allocators may be *added* in minor releases. See [API stability](#-api-stability) and the [CHANGELOG](CHANGELOG.md).

---

## ✨ Highlights

- **Concept-driven contract.** Every allocator satisfies the `Allocator` concept (`allocate` / `deallocate`); capabilities that not all allocators can provide are modeled as refinements — `ResettableAllocator` (`reset()`) and `OwningAllocator` (`owns()`). Compositions are constrained on these concepts, so misuse is a compile error with a readable message rather than a runtime surprise.
- **Composable by design.** Wrap any allocator with decorators — bounds-checking canaries, tagged tracking, a mutex, or a fallback path — without touching the underlying allocator.
- **Alignment-correct.** Over-aligned types (e.g. `alignas(64)`) are handled throughout, and the whole suite runs clean under ASan/UBSan in CI.
- **STL, PMR & language integration.** `StlAdapter` plugs any allocator into standard containers, `PmrAdapter` exposes one as a `std::pmr::memory_resource`, and `New`/`Delete` helpers plus an `operator new`/`delete` injection macro cover object lifetimes.
- **Verified.** Built and tested on Linux, Windows and macOS in Debug and Release, with a dedicated AddressSanitizer + UndefinedBehaviorSanitizer run, and every public header compiled in isolation.

## 🧱 Architecture

```
        Adapters:   StlAdapter · PmrAdapter · New/Delete · MEMCORE_ENABLE_CLASS_ALLOCATOR
                                     |
        Decorators: Tracker · Canary · ThreadSafe · Fallback   (wrap an Allocator)
                                     |
        Allocators: Linear · Stack · Pool · Arena · FreeList   (manage a region)
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
| `FreeListAllocator` | O(n) first-fit | O(1) + coalescing | General-purpose, variable sizes |

### Decorators (wrap any `Allocator`)
| Decorator | Adds |
|---|---|
| `TrackerAllocator` | Per-tag usage & peak stats, leak reporting |
| `CanaryAllocator` | Front/back guard bytes; aborts on over/underflow |
| `ThreadSafeAllocator` | Mutex around a wrapped allocator |
| `FallbackAllocator` | Primary allocator with a fallback on failure |

### Adapters
| Adapter | Purpose |
|---|---|
| `StlAdapter<T, A>` | Use any allocator with an STL container |
| `PmrAdapter<A>` | Expose any allocator as a `std::pmr::memory_resource` |
| `New` / `Delete` | Construct/destroy a single object through an allocator |
| `MEMCORE_ENABLE_CLASS_ALLOCATOR` | Route a class's `new`/`delete` to an allocator |

## 📜 The contract

These semantics are part of the public API and are stable across `1.x`. The
authoritative version lives next to the concepts in
[`AllocatorConcept.hpp`](include/MemCore/AllocatorConcept.hpp).

| Operation | Guarantee |
|---|---|
| `allocate(size, alignment)` | `alignment` must be a power of two; the result is aligned to at least that. Returns `{nullptr, 0}` on failure — **never throws**. A zero `size` returns `{nullptr, 0}`. `Block::size` may exceed the request. |
| `deallocate(ptr, size)` | **Sized deallocation:** `size` must be the size originally requested for `ptr` (like C++14 sized `delete`). Most allocators ignore it, but `StackAllocator` and `VirtualUpstream` genuinely need it. `nullptr` is a no-op. Never throws. |
| `reset()` | Releases everything at once and **invalidates every outstanding pointer**. Destructors are *not* run — ensure no live objects remain. |
| `owns(ptr)` | True iff `ptr` came from this allocator. Not universally available: the OS passthroughs (`MallocUpstream`, `VirtualUpstream`) own no contiguous range and deliberately do **not** model `OwningAllocator`. |

**Construction** falls into three deliberate categories:

1. **Sources** (`MallocUpstream`, `VirtualUpstream`) — default-constructed; take memory from the OS.
2. **Region allocators** (`Linear`, `Stack`, `Pool`, `FreeList`) — built from a `Block` they do not own and must not outlive. **Non-copyable** (a copy would alias the region) but movable; a moved-from allocator is empty, so `allocate()` returns `nullptr` and `owns()` returns `false`.
3. **Wrappers** (`Arena`, `Canary`, `Tracker`, `ThreadSafe`, `Fallback`) — built from a reference to the allocator they build on, which must outlive them.

> ⚠️ **Lifetime ordering.** Anything still *holding* memory from an allocator must be destroyed before the allocator's backing region is released. This is easy to get wrong with containers, whose destructor returns memory to the allocator:
>
> ```cpp
> MemCore::Block chunk = upstream.allocate(64 * 1024, 16);
> {
>     MemCore::FreeListAllocator heap(chunk);
>     MemCore::PmrAdapter resource(heap);
>     std::pmr::vector<int> v(&resource);
>     // ...
> } // v destroyed here, while `chunk` is still valid  ✅
> upstream.deallocate(chunk.ptr, chunk.size);
> ```
>
> Releasing `chunk` *before* `v` goes out of scope is a use-after-free.

## 🧵 Thread safety

**No allocator is thread-safe on its own** — that is a deliberate design choice, so single-threaded users pay nothing. For concurrent use, wrap one in `ThreadSafeAllocator`:

```cpp
MemCore::PoolAllocator pool(chunk, sizeof(Particle), alignof(Particle));
MemCore::ThreadSafeAllocator<MemCore::PoolAllocator> safe(pool);
// safe.allocate(...) may now be called from multiple threads
```

`ThreadSafeAllocator` is a full `Allocator`, so it composes anywhere — inside a decorator, as an `Arena` upstream, or behind `StlAdapter`/`PmrAdapter`. It takes any mutex type (`std::mutex` by default), so a spinlock can be substituted. Its operations are `noexcept`: a mutex failure is unrecoverable for an allocator and terminates rather than being reported.

`TrackerAllocator`'s statistics are atomic, but the allocation itself is only thread-safe if the wrapped allocator is.

## 🔒 API stability

MemCore `1.0.0` follows [semantic versioning](https://semver.org):

- **Patch** (`1.0.x`) — bug fixes only.
- **Minor** (`1.x.0`) — additive, source-compatible changes: new allocators, new decorators, new helpers.
- **Major** (`2.0.0`) — required for any breaking change to the contract above, the concepts, or existing signatures.

The concepts are load-bearing, so contract violations surface as compile errors. `static_assert`s in the test suite pin the conformance of every type, and the documented behaviours (zero-size, moved-from state, ownership honesty) are covered by tests so the docs cannot drift from the code.

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
find_package(MemCore 1.0 REQUIRED)
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

### Standard `pmr` containers

```cpp
#include <memory_resource>
#include <MemCore/FreeListAllocator.hpp>
#include <MemCore/PmrAdapter.hpp>

MemCore::FreeListAllocator heap(chunk);   // reclaims space, any order
MemCore::PmrAdapter resource(heap);

std::pmr::vector<int> v(&resource);       // allocates from `heap`
std::pmr::string s("hello", &resource);
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

## 📊 Performance

Measured with the bundled Google Benchmark suite (`-DMEMCORE_BUILD_BENCHMARKS=ON`).
Numbers vary by machine — run it yourself rather than trusting these.

| Workload | MemCore | `malloc` | `std::pmr` |
|---|---|---|---|
| Transient burst (10k objects, bulk release) | `Linear` **25.5 µs** | 252 µs | 27.9 µs |
| Fixed-size churn (alloc + free) | `Pool` **1.5 ns** | 15.6 ns | — |
| Variable-size fragmenting churn | `FreeList` 17.6 ns | 16.6 ns | — |

The specialized allocators are ~10× faster than `malloc` for the patterns they
target. `FreeListAllocator` is deliberately *on par* with `malloc` on general
churn — it competes with a heavily-tuned general allocator while remaining a
deterministic, self-contained region allocator.

## 🗺️ Roadmap

**Shipped in 1.0:** aligned OS upstreams; linear, stack, pool, arena and
coalescing free-list allocators; STL, PMR and class adapters; tagged tracking;
canary debugging; fallback and thread-safe decorators; a concept-based contract;
install/`find_package` packaging; benchmarks; and CI across three platforms with
sanitizers.

**Planned for 1.x** (all additive, no breaking changes): slab allocator with size
classes, buddy allocator, allocation visualization, and a dedicated lock-free
concurrency layer.

## 📜 License

MemCore is licensed under the MIT License. See [LICENSE](LICENSE).
