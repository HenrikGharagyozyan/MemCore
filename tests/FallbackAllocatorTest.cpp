#include <gtest/gtest.h>

#include <MemCore/FallbackAllocator.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>


TEST(FallbackAllocatorTest, SwitchesToFallbackOnOOM) 
{
    MemCore::MallocUpstream system_malloc;
    
    // Создаем крошечный буфер на 128 байт
    MemCore::Block chunk = system_malloc.allocate(128, 8);
    
    {
        MemCore::LinearAllocator primary(chunk);
        
        // Комбинируем: Линейный (быстрый) + Malloc (медленный, но безлимитный)
        MemCore::FallbackAllocator<MemCore::LinearAllocator, MemCore::MallocUpstream> fallback_alloc(primary, system_malloc);

        // 1. Запрашиваем 100 байт. Это должно уместиться в primary
        MemCore::Block a = fallback_alloc.allocate(100, 8);
        EXPECT_NE(a.ptr, nullptr);
        EXPECT_TRUE(primary.owns(a.ptr)); // Проверяем, что память выдал Линейный

        // 2. Запрашиваем еще 50 байт. В primary осталось всего 28 байт!
        // Он вернет nullptr, и FallbackAllocator должен перехватить вызов 
        // и передать его в system_malloc.
        MemCore::Block b = fallback_alloc.allocate(50, 8);
        EXPECT_NE(b.ptr, nullptr);
        EXPECT_FALSE(primary.owns(b.ptr)); // Проверяем, что это УЖЕ НЕ Линейный аллокатор

        // Освобождаем (внутри FallbackAllocator сам разберется, куда отправлять deallocate)
        fallback_alloc.deallocate(a.ptr, a.size);
        fallback_alloc.deallocate(b.ptr, b.size);
    }

    system_malloc.deallocate(chunk.ptr, chunk.size);
}