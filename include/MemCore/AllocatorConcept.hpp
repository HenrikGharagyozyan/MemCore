#pragma once

#include "Types.hpp"

#include <concepts>

namespace MemCore
{

    // Concept for an allocator. Any class that wants to be an allocator in our library
    // must implement these three methods with exactly the same signatures.
    template <typename T>
    concept Allocator = requires(T a, std::size_t size, std::size_t alignment, void* ptr) 
    {
        // 1. The allocate method must take a size and an alignment,
        // and return our Block structure.
        { a.allocate(size, alignment) } -> std::same_as<Block>;

        // 2. The deallocate method must free memory.
        // noexcept means it is guaranteed not to throw an exception (crash).
        { a.deallocate(ptr, size) } noexcept;

        // 3. The reset method must immediately free ALL memory owned by the allocator.
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