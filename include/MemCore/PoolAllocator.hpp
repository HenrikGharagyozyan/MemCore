#pragma once
#include "AllocatorConcept.hpp"
#include "Align.hpp"
#include <cstddef>
#include <cassert>

namespace MemCore 
{

    class PoolAllocator 
    {
    private:
        // Технический узел, который живет внутри СВОБОДНЫХ блоков памяти.
        // Как только блок отдается пользователю, эта структура затирается его данными.
        struct FreeNode 
        {
            FreeNode* next;
        };

        Block m_memory;
        std::size_t m_chunk_size;
        std::size_t m_alignment;
        FreeNode* m_free_list;

        // Внутренняя функция для первичной нарезки сырой памяти на равные куски
        void initialize_free_list() noexcept 
        {
            m_free_list = nullptr;

            std::byte* base = static_cast<std::byte*>(m_memory.ptr);
            std::size_t memory_size = m_memory.size;

            // 1. Выравниваем стартовый адрес
            void* aligned_base = AlignForward(base, m_alignment);
            std::byte* current = static_cast<std::byte*>(aligned_base);
            
            // Считаем, сколько байт потеряли на выравнивании начала
            std::size_t shift = current - base;
            if (shift >= memory_size) 
                return; 
            
            std::size_t available_size = memory_size - shift;

            // 2. Размер чанка должен быть не меньше, чем размер указателя FreeNode!
            std::size_t actual_chunk_size = m_chunk_size;
            if (actual_chunk_size < sizeof(FreeNode)) 
            {
                actual_chunk_size = sizeof(FreeNode);
            }
            
            // 3. Размер чанка обязан быть кратен выравниванию, чтобы ВСЕ чанки были выровнены
            std::size_t remainder = actual_chunk_size % m_alignment;
            if (remainder != 0) 
            {
                actual_chunk_size += (m_alignment - remainder);
            }

            // 4. Считаем, сколько таких чанков влезет в доступную память
            std::size_t num_chunks = available_size / actual_chunk_size;
            if (num_chunks == 0) 
                return;

            // 5. Нарезаем память и собираем из неё связанный список свободных блоков
            // Идем с конца, чтобы первый блок в памяти стал первой "головой" списка
            for (std::size_t i = num_chunks; i > 0; --i) 
            {
                std::byte* chunk_ptr = current + ((i - 1) * actual_chunk_size);
                FreeNode* node = reinterpret_cast<FreeNode*>(chunk_ptr);
                
                // Добавляем текущий узел в начало списка
                node->next = m_free_list;
                m_free_list = node;
            }
        }

    public:
        // Пул принимает сырой кусок памяти, размер одного элемента и требуемое выравнивание
        PoolAllocator(Block memory, std::size_t chunk_size, std::size_t alignment) noexcept
            : m_memory(memory), m_chunk_size(chunk_size), m_alignment(alignment), m_free_list(nullptr) 
        {
            // Выравнивание должно быть степенью двойки (проверка битовыми операциями)
            assert(alignment != 0 && (alignment & (alignment - 1)) == 0);
            initialize_free_list();
        }

        Block allocate(std::size_t size, std::size_t alignment) noexcept 
        {
            // Пул жесткий: он может выдать только свой фиксированный размер и выравнивание
            if (size > m_chunk_size || alignment > m_alignment || !m_free_list) 
            {
                return { nullptr, 0 };
            }

            // Берем первый свободный блок (О(1))
            FreeNode* node = m_free_list;
            m_free_list = m_free_list->next;

            return { static_cast<void*>(node), m_chunk_size };
        }

        // В отличие от Linear и Arena, Пул умеет освобождать любые блоки в любом порядке!
        void deallocate(void* ptr, std::size_t /*size*/) noexcept 
        {
            if (!ptr) 
                return;

            // Просто возвращаем блок в список свободных (О(1))
            FreeNode* node = static_cast<FreeNode*>(ptr);
            node->next = m_free_list;
            m_free_list = node;
        }

        // Быстрый сброс всей памяти разом
        void reset() noexcept 
        {
            initialize_free_list();
        }
    };

    static_assert(Allocator<PoolAllocator>);

}