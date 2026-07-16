#pragma once

#include "AllocatorConcept.hpp"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>


namespace MemCore 
{

    template <typename Allocator>
    class CanaryAllocator 
    {
    private:
        Allocator& m_allocator;

        // Канарейки (Магические числа)
        static constexpr std::uint32_t FRONT_MAGIC = 0xDEADBEEF;
        static constexpr std::uint32_t BACK_MAGIC  = 0xBAADF00D;

        // Выравниваем заголовок на 16 байт. Это нужно, чтобы пользовательские 
        // данные, идущие сразу за ним, сохранили правильное выравнивание.
        struct alignas(16) Header 
        {
            std::size_t user_size;

            // Явно заполняем пустоту, чтобы magic всегда был в самом конце заголовка.
            // На 64-битных системах это будет 4 байта, на 32-битных — 8 байт.
            std::byte padding[16 - sizeof(std::size_t) - sizeof(std::uint32_t)]; // Padding to ensure alignment

            std::uint32_t magic;
        };

    public:
        explicit CanaryAllocator(Allocator& allocator) noexcept
            : m_allocator(allocator) 
        {
        }

        Block allocate(std::size_t size, std::size_t alignment) 
        {
            // Просим память под: Заголовок + Данные + Задняя канарейка
            std::size_t total_size = sizeof(Header) + size + sizeof(std::uint32_t);
            
            // Гарантируем, что базовый аллокатор выровняет хотя бы по границе заголовка
            std::size_t actual_align = (alignment > alignof(Header)) ? alignment : alignof(Header);

            Block block = m_allocator.allocate(total_size, actual_align);
            if (!block.ptr) 
                return { nullptr, 0 };

            // 1. Ставим Front Canary
            Header* header = static_cast<Header*>(block.ptr);
            header->user_size = size;
            header->magic = FRONT_MAGIC;

            // 2. Вычисляем начало пользовательских данных
            std::byte* payload = reinterpret_cast<std::byte*>(block.ptr) + sizeof(Header);

            // 3. Ставим Back Canary в самом конце.
            // Используем memcpy вместо прямого каста, чтобы избежать 
            // краша на ARM процессорах при невыровненном доступе к памяти!
            std::memcpy(payload + size, &BACK_MAGIC, sizeof(BACK_MAGIC));

            return { payload, size };
        }

        void deallocate(void* ptr, std::size_t size) 
        {
            if (!ptr) 
                return;

            std::byte* payload = static_cast<std::byte*>(ptr);
            Header* header = reinterpret_cast<Header*>(payload - sizeof(Header));

            // Проверка 1: Front Canary (Защита от записи до массива)
            if (header->magic != FRONT_MAGIC) 
            {
                std::cerr << "[MemCore FATAL] Front Canary corrupted! Buffer Underflow at " << ptr << "\n";
                std::abort(); // Мгновенно убиваем программу!
            }

            // Проверка 2: Back Canary (Защита от переполнения массива)
            std::uint32_t back_magic;
            std::memcpy(&back_magic, payload + header->user_size, sizeof(BACK_MAGIC));

            if (back_magic != BACK_MAGIC) 
            {
                std::cerr << "[MemCore FATAL] Back Canary corrupted! Buffer Overflow at " << ptr << "\n";
                std::abort(); // Мгновенно убиваем программу!
            }

            // Если всё чисто — возвращаем память реальному аллокатору
            std::size_t total_size = sizeof(Header) + header->user_size + sizeof(std::uint32_t);
            m_allocator.deallocate(header, total_size);
        }
        
        bool owns(const void* ptr) const noexcept 
        {
            if (!ptr) 
                return false;

            const std::byte* payload = static_cast<const std::byte*>(ptr);
            const Header* header = reinterpret_cast<const Header*>(payload - sizeof(Header));
            return m_allocator.owns(header);
        }
    };

} // namespace MemCore