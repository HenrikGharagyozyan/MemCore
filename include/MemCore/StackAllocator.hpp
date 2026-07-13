#pragma once
#include "AllocatorConcept.hpp"
#include "Align.hpp"
#include <cstddef>
#include <cstdint>
#include <cassert>

namespace MemCore 
{

    class StackAllocator 
    {
    private:
        // Техническая структура, которая будет невидимо жить перед пользовательской памятью
        struct Header 
        {
            std::size_t previous_offset;
        };

        Block m_memory;        // Backing store (сырая память от Upstream)
        std::size_t m_offset;  // Текущее положение ползунка

    public:
        using Marker = std::size_t;

        explicit StackAllocator(Block memory) noexcept 
            : m_memory(memory)
            , m_offset(0) 
        {
        }

        Block allocate(std::size_t size, std::size_t alignment) noexcept 
        {
            std::byte* base = static_cast<std::byte*>(m_memory.ptr);
            
            // 1. Считаем, где минимально могут начаться пользовательские данные.
            // Они должны идти как минимум после текущего смещения + размер заголовка.
            std::byte* earliest_payload_ptr = base + m_offset + sizeof(Header);
            
            // 2. Выравниваем указатель пользователя под его требования
            void* aligned_payload_ptr = AlignForward(earliest_payload_ptr, alignment);
            std::byte* payload_ptr = static_cast<std::byte*>(aligned_payload_ptr);
            
            // 3. Заголовок должен лежать строго ПЕРЕД выровненным указателем пользователя
            std::byte* header_ptr = payload_ptr - sizeof(Header);
            
            // 4. Проверяем, хватает ли у нас памяти во всем чанке
            std::size_t new_offset = (payload_ptr + size) - base;
            if (new_offset > m_memory.size) 
            {
                return { nullptr, 0 }; // Память переполнена
            }

            // 5. Записываем информацию в заголовок. 
            // Используем placement new для конструирования структуры в сырой памяти.
            Header* header = ::new (static_cast<void*>(header_ptr)) Header();
            header->previous_offset = m_offset;

            // 6. Сдвигаем ползунок на новое место
            m_offset = new_offset;

            return { aligned_payload_ptr, size };
        }

        // Точечное освобождение памяти (должно происходить строго в обратном порядке!)
        void deallocate(void* ptr, std::size_t size) noexcept 
        {
            if (!ptr) 
                return;

            std::byte* base = static_cast<std::byte*>(m_memory.ptr);
            std::byte* payload_ptr = static_cast<std::byte*>(ptr);

            // Проверка на LIFO: освобождаемый блок обязан быть самым верхним в стеке!
            // То есть текущий m_offset должен в точности равняться концу этого блока.
            assert((payload_ptr + size) - base == m_offset && "Out-of-order deallocation in StackAllocator! LIFO violated.");

            // Отступаем назад, чтобы прочитать заголовок
            std::byte* header_ptr = payload_ptr - sizeof(Header);
            Header* header = reinterpret_cast<Header*>(header_ptr);

            // Просто откатываем ползунок назад к состоянию до этой аллокации
            m_offset = header->previous_offset;
        }

        // Дополнительный профессиональный API: позволяет сделать "закладку" 
        // и откатиться к ней разом, удалив целую пачку объектов.
        Marker get_marker() const noexcept 
        {
            return m_offset;
        }

        void free_to_marker(Marker marker) noexcept 
        {
            assert(marker <= m_offset && "Cannot rollback to a forward marker!");
            m_offset = marker;
        }

        void reset() noexcept 
        {
            m_offset = 0;
        }
    };

    static_assert(Allocator<StackAllocator>);

}