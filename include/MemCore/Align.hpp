#pragma once
#include <cstddef>
#include <cstdint>
#include <bit>     
#include <cassert>

namespace MemCore 
{

    inline bool IsAligned(const void* ptr, std::size_t alignment) noexcept 
    {
        assert(std::has_single_bit(alignment) && "Alignment must be a power of two");

        // Convert the pointer to an integer so arithmetic can be performed
        auto address = reinterpret_cast<std::uintptr_t>(ptr);
        
        // If the lower bits are zero, the address is divisible by alignment
        return (address & (alignment - 1)) == 0;
    }

    // Moves the pointer FORWARD to the nearest aligned address
    inline void* AlignForward(void* ptr, std::size_t alignment) noexcept 
    {
        assert(std::has_single_bit(alignment) && "Alignment must be a power of two");

        auto address = reinterpret_cast<std::uintptr_t>(ptr);
        
        auto aligned_address = (address + alignment - 1) & ~(alignment - 1);
        
        return reinterpret_cast<void*>(aligned_address);
    }

}