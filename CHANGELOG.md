# Changelog

All notable changes to MemCore are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] — 2026-07-20

First stable release. The public API documented in the README and in
`AllocatorConcept.hpp` is now frozen: no breaking changes within `1.x`.

### Added

- **`FreeListAllocator`** — general-purpose allocator over a fixed region with
  variable-size allocation, free-in-any-order, and coalescing of adjacent free
  blocks via boundary tags.
- **`PmrAdapter`** — exposes any MemCore allocator as a
  `std::pmr::memory_resource`, so every `std::pmr` container can allocate from
  it. Guarded on `__has_include(<memory_resource>)`.
- **Packaging** — `install()`/export rules and a package config, so
  `find_package(MemCore)` works; `MemCore::MemCore` imported target.
- **`MEMCORE_BUILD_TESTS`** / **`MEMCORE_INSTALL`** / **`MEMCORE_BUILD_BENCHMARKS`**
  options, defaulting so that consuming MemCore as a subproject builds neither
  the tests nor googletest.
- **Benchmarks** — Google Benchmark suite comparing against `malloc` and
  `std::pmr::monotonic_buffer_resource`.
- **CI** — build/test matrix over Linux, Windows and macOS in Debug and Release,
  plus a dedicated AddressSanitizer + UndefinedBehaviorSanitizer job.
- **Header self-containment check** — every public header is compiled in
  isolation as part of the build.
- `ResettableAllocator` and `OwningAllocator` concept refinements.
- Documented contract semantics next to the concepts.

### Changed

- **`Allocator` concept narrowed** to `{ allocate, deallocate }`; `reset()` and
  `owns()` moved into the `ResettableAllocator` / `OwningAllocator`
  refinements. Decorators satisfy the base concept as a result.
- **Composition templates are concept-constrained.** `FallbackAllocator` now
  requires an `OwningAllocator` primary, since its deallocation routing depends
  on `owns()`.
- **Decorators advertise capabilities honestly.** A decorator models
  `OwningAllocator`/`ResettableAllocator` only when the allocator it wraps does.
- **`allocate` is `noexcept`** across the library and in the concept: exhaustion
  is reported by returning `{nullptr, 0}`, never by throwing.
- **Zero-size allocation is uniform** — `allocate(0, n)` returns `{nullptr, 0}`
  for every allocator (previously half returned a non-null pointer).
- **Copy/move semantics** — stateful region allocators (`Linear`, `Stack`,
  `Pool`, `FreeList`) are non-copyable (a copy would alias the region) and
  movable, with a defined empty moved-from state.
- `StlAdapter`'s allocator pointer is private instead of public.
- `[[nodiscard]]` applied uniformly to `allocate()`.
- Version reported as `1.0.0`; README rewritten to match the implementation.

### Fixed

- **`MallocUpstream` portability** — `std::aligned_alloc` does not exist in the
  MSVC runtime and is UB for alignments below `sizeof(void*)`. Now routes
  through a platform-matched aligned alloc/free pair with an alignment floor.
- **Over-aligned payloads in `CanaryAllocator` and `TrackerAllocator`** — a
  fixed header offset broke alignments greater than 16; the payload is now
  aligned by forward shift with the offset recorded in the header.
- **`ArenaAllocator` block growth** — a large-alignment request could be shifted
  past the end of a freshly grown block; block sizing now budgets alignment
  padding, with a defensive capacity re-check.
- **`StackAllocator` LIFO safety** — an out-of-order deallocation corrupted the
  cursor under `NDEBUG`, where the assertion was compiled out. It now asserts in
  debug and is a safe no-op in release.
- **`PoolAllocator` chunk alignment** — chunks were aligned only to the caller's
  request, so a smaller alignment than `alignof(FreeNode)` made the intrusive
  free-list write misaligned (undefined behaviour, found via UBSan). Chunk
  layout now widens to the free-list node's alignment.
- **`ThreadSafeAllocator` satisfied no concept**, so it could not be used with
  `StlAdapter`, as an `Arena` upstream, or inside any decorator. Its operations
  are now `noexcept`.
- **`VirtualUpstream::owns()` was dishonest** — it returned `ptr != nullptr`,
  claiming every non-null pointer. Removed, matching `MallocUpstream`.
- **`StackAllocator.hpp` was not self-contained** (missing `<new>`).
