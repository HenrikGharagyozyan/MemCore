#pragma once
#include "AllocatorConcept.hpp"

#include <cstddef>
#include <new>


/**
 * @brief Макрос для внедрения кастомного аллокатора прямо в класс.
 * Размещать строго в `public:` секции класса.
 * 
 * @param Type Тип самого класса (например, Bullet).
 * @param AllocatorInstance Выражение, возвращающее ссылку на аллокатор (например, GetPool()).
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