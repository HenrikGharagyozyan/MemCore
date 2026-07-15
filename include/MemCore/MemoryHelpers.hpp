#pragma once
#include "AllocatorConcept.hpp"

#include <utility>
#include <new>

namespace MemCore 
{

    /**
     * @brief Allocates memory through the allocator and invokes the object's constructor.
     * 
     * @tparam T The type of the object to create.
     * @tparam Allocator The allocator type.
     * @tparam Args The constructor argument types.
     * @param allocator A reference to the allocator.
     * @param args Arguments for the constructor of T.
     * @return T* Pointer to the created object or nullptr if memory is unavailable.
     */
    template <typename T, typename Allocator, typename... Args>
    [[nodiscard]] T* New(Allocator& allocator, Args&&... args) 
    {
        // 1. Allocate raw memory (accounting for the size and alignment of T)
        Block block = allocator.allocate(sizeof(T), alignof(T));
        
        if (!block.ptr) 
        {
            return nullptr; // Out of memory
        }

        // 2. Create the object in the allocated memory with placement new
        // std::forward preserves lvalue/rvalue references for the arguments
        return ::new (block.ptr) T(std::forward<Args>(args)...);
    }

    /**
     * @brief Invokes the object's destructor and returns the memory to the allocator.
     * 
     * @tparam T The type of the object to delete.
     * @tparam Allocator The allocator type.
     * @param allocator A reference to the allocator.
     * @param ptr Pointer to the object to delete.
     */
    template <typename T, typename Allocator>
    void Delete(Allocator& allocator, T* ptr) noexcept 
    {
        if (!ptr) 
        {
            return;
        }

        // 1. Explicitly call the object's destructor
        ptr->~T();

        // 2. Return the raw memory to the allocator
        allocator.deallocate(ptr, sizeof(T));
    }

} // namespace MemCore