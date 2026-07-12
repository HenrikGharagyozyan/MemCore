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

}