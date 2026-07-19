#pragma once

#include "Types.hpp"

#include <concepts>

namespace MemCore
{

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
