#pragma once

#include "Types.hpp"

#include <concepts>

namespace MemCore
{

    // =========================== THE CONTRACT ===========================
    //
    // The concepts below name the operations; this block defines what they
    // MEAN. These semantics are part of MemCore's public API.
    //
    // Block allocate(size, alignment)
    //   * `alignment` must be a power of two. The returned pointer is aligned
    //     to at least `alignment`.
    //   * Returns { nullptr, 0 } on failure -- allocators report exhaustion by
    //     returning null, never by throwing.
    //   * A zero `size` returns { nullptr, 0 } (uniformly, for every allocator).
    //   * Block::size is the usable size and may exceed the requested size.
    //
    // void deallocate(ptr, size) noexcept        [SIZED DEALLOCATION]
    //   * `size` MUST be the size originally requested from allocate() for
    //     `ptr`. This mirrors C++14's sized operator delete.
    //   * Most allocators ignore it and recover the size from their own
    //     metadata, but some genuinely need it:
    //       - StackAllocator uses it to verify LIFO order.
    //       - VirtualUpstream uses it to unmap the correct length.
    //     Passing a size that does not match is undefined behaviour.
    //   * Passing nullptr is always a no-op. Never throws.
    //
    // void reset() noexcept                      [ResettableAllocator]
    //   * Releases everything the allocator handed out at once.
    //   * INVALIDATES every outstanding pointer from this allocator. Destructors
    //     are NOT run -- the caller must ensure no live objects remain. Using a
    //     pointer obtained before reset() is undefined behaviour.
    //
    // bool owns(ptr) const                       [OwningAllocator]
    //   * True iff `ptr` was handed out by this allocator (see OwningAllocator
    //     below for why this is not universally implementable).
    //   * owns(nullptr) is false.
    //
    // CONSTRUCTION -- three deliberate categories:
    //   1. Sources (MallocUpstream, VirtualUpstream): default-constructed, take
    //      memory from the OS. Not owning: they answer no ownership queries.
    //   2. Region allocators (Linear, Stack, Pool, FreeList): constructed from a
    //      Block they do not own and must not outlive. Non-copyable (a copy
    //      would alias the region) but movable; a moved-from allocator is empty,
    //      so allocate() returns nullptr and owns() returns false.
    //   3. Wrappers (Arena, Canary, Tracker, ThreadSafe, Fallback): constructed
    //      from a reference to the allocator they build on, which must outlive
    //      them.
    //
    // THREAD SAFETY: no allocator is thread-safe on its own. Wrap it in
    // ThreadSafeAllocator for concurrent use.
    // ====================================================================

    // The minimal allocator contract: every allocator can hand out and reclaim
    // raw, aligned memory. Capabilities that only SOME allocators can provide
    // (resetting everything at once, answering ownership) are modeled as
    // refinements below rather than forced into this base. Keeping the base
    // small is what lets decorators (Canary, Tracker, Fallback) qualify as
    // allocators even though they leave reset()/owns() to the wrapped layer.
    template <typename T>
    concept Allocator = requires(T a, std::size_t size, std::size_t alignment, void* ptr)
    {
        // Take a size and an alignment, return our Block structure.
        { a.allocate(size, alignment) } -> std::same_as<Block>;

        // Free memory. Deallocation must never throw (like a destructor).
        { a.deallocate(ptr, size) } noexcept;
    }; 

    // An allocator that can release ALL of its memory at once. Natural for
    // arena/stack/linear/pool styles and a (legal) no-op for a system malloc
    // wrapper. Decorators deliberately do NOT model this: they satisfy the base
    // Allocator but forward any real reset to the layer they wrap.
    template <typename T>
    concept ResettableAllocator = Allocator<T> && requires(T a)
    {
        { a.reset() } noexcept;
    };

    // An allocator that can additionally answer whether a given pointer belongs
    // to it. This is a cheap range check for allocators backed by a single
    // contiguous region (Linear, Stack, Pool, Arena), but it is NOT universally
    // implementable: a thin OS passthrough such as MallocUpstream owns no
    // contiguous range and therefore deliberately does not model this concept.
    //
    // Composers that must route a pointer to the right child by ownership
    // (e.g. FallbackAllocator's primary leg) constrain on OwningAllocator so
    // the type system rejects a non-owning allocator at that position.
    template <typename T>
    concept OwningAllocator = Allocator<T> && requires(const T a, const void* ptr)
    {
        { a.owns(ptr) } -> std::same_as<bool>;
    };

}
