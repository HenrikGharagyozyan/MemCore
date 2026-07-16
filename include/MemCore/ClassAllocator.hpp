#pragma once
#include "AllocatorConcept.hpp"

#include <cstddef>
#include <new>


/**
 * @brief Macro for injecting a custom allocator directly into a class.
 * Place it strictly in the `public:` section of the class.
 * 
 * @param Type The class type itself (for example, Bullet).
 * @param AllocatorInstance An expression that returns a reference to the allocator (for example, GetPool()).
 */
#define MEMCORE_ENABLE_CLASS_ALLOCATOR(Type, AllocatorInstance) \
    static void* operator new(std::size_t size) \
    { \
        MemCore::Block block = (AllocatorInstance).allocate(size, alignof(Type)); \
        if (!block.ptr) \
        { \
            throw std::bad_alloc(); \
        } \
        return block.ptr; \
    } \
    static void operator delete(void* ptr, std::size_t size) noexcept \
    { \
        (AllocatorInstance).deallocate(ptr, size); \
    }