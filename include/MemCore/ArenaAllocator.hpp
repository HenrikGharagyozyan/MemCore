#pragma once
#include "AllocatorConcept.hpp"
#include "Align.hpp"
#include <cstddef>
#include <algorithm>
#include <new>

namespace MemCore 
{

    template <typename Upstream>
    class ArenaAllocator 
    {
    private:
        // Узел интрузивного списка, который будет жить в начале каждого большого чанка
        struct BlockNode 
        {
            BlockNode* next;
            std::size_t capacity;
            std::size_t offset;
        };

        Upstream& m_upstream;            // Источник сырой памяти
        std::size_t m_default_block_size;// Размер чанка по умолчанию (например, 4КБ)
        BlockNode* m_head;               // Указатель на текущий активный чанк

        // Внутренний метод для запроса нового чанка у Upstream
        BlockNode* allocate_new_block(std::size_t min_size) noexcept 
        {
            // Нам нужно уместить размер объекта + заголовок нашего узла списка
            std::size_t required_size = min_size + sizeof(BlockNode);
            std::size_t size_to_alloc = std::max(required_size, m_default_block_size);

            // Запрашиваем память у Upstream с выравниванием под наш заголовок
            Block upstream_block = m_upstream.allocate(size_to_alloc, alignof(BlockNode));
            if (!upstream_block.ptr) 
            {
                return nullptr;
            }

            // Размещаем заголовок узла в самом начале выделенной памяти (placement new)
            BlockNode* node = ::new (upstream_block.ptr) BlockNode{
                .next = m_head,
                .capacity = upstream_block.size,
                .offset = sizeof(BlockNode) // Занято под сам заголовок
            };

            // Делаем этот блок текущим активным
            m_head = node;
            return node;
        }

    public:
        explicit ArenaAllocator(Upstream& upstream, std::size_t default_block_size = 4096) noexcept
            : m_upstream(upstream)
            , m_default_block_size(default_block_size)
            , m_head(nullptr) 
        {
        }

        // При уничтожении Арены мы обязаны вернуть ВСЕ чанки обратно в Upstream
        ~ArenaAllocator() noexcept 
        {
            reset();
        }

        // Запрещаем копирование, чтобы случайно не размножить указатели на одни и те же блоки
        ArenaAllocator(const ArenaAllocator&) = delete;
        ArenaAllocator& operator=(const ArenaAllocator&) = delete;

        // Разрешаем перемещение
        ArenaAllocator(ArenaAllocator&& other) noexcept
            : m_upstream(other.m_upstream), 
              m_default_block_size(other.m_default_block_size), 
              m_head(other.m_head) 
        {
            other.m_head = nullptr;
        }

        Block allocate(std::size_t size, std::size_t alignment) noexcept 
        {
            if (size == 0) 
                return { nullptr, 0 };

            // Если блоков еще нет — создаем первый
            if (!m_head) 
            {
                if (!allocate_new_block(size)) 
                    return { nullptr, 0 };
            }

            BlockNode* current = m_head;
            std::byte* base = reinterpret_cast<std::byte*>(current);
            std::byte* current_ptr = base + current->offset;

            // Вычисляем выравнивание пользовательского указателя
            void* aligned_ptr = AlignForward(current_ptr, alignment);
            std::byte* payload_ptr = static_cast<std::byte*>(aligned_ptr);
            std::size_t shift = payload_ptr - current_ptr;

            // Если в текущем чанке не хватает места, выделяем новый чанк
            if (current->offset + shift + size > current->capacity) 
            {
                current = allocate_new_block(size);
                if (!current) 
                    return { nullptr, 0 };

                // Пересчитываем указатели для нового чанка
                base = reinterpret_cast<std::byte*>(current);
                current_ptr = base + current->offset;
                aligned_ptr = AlignForward(current_ptr, alignment);
                payload_ptr = static_cast<std::byte*>(aligned_ptr);
                shift = payload_ptr - current_ptr;
            }

            // Двигаем ползунок текущего чанка вперед
            current->offset = (payload_ptr + size) - base;
            return { aligned_ptr, size };
        }

        // Как и линейный аллокатор, Арена игнорирует точечный deallocate
        void deallocate(void* /*ptr*/, std::size_t /*size*/) noexcept {}

        // Полный сброс: проходим по интрузивному списку и освобождаем все чанки через Upstream
        void reset() noexcept 
        {
            BlockNode* current = m_head;
            while (current) 
            {
                BlockNode* next = current->next;
                
                void* ptr = static_cast<void*>(current);
                std::size_t cap = current->capacity;
                
                m_upstream.deallocate(ptr, cap);
                current = next;
            }
            m_head = nullptr;
        }
    };

}