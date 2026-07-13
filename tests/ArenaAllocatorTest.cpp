#include <gtest/gtest.h>

#include <MemCore/ArenaAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>


TEST(ArenaAllocatorTest, GrowthAndReset) 
{
    MemCore::MallocUpstream upstream;
    
    // Создаем арену с крошечным размером блока по умолчанию (64 байта), 
    // чтобы легко спровоцировать её рост
    MemCore::ArenaAllocator arena(upstream, 64);

    // 1. Первая аллокация
    MemCore::Block a = arena.allocate(32, 8);
    EXPECT_NE(a.ptr, nullptr);

    // 2. Вторая аллокация: 32 байта (А) + размер заголовка интрузивного списка 
    // уже не влезут в стартовые 64 байта чанка. Арена должна автоматически вырасти!
    MemCore::Block b = arena.allocate(40, 8);
    EXPECT_NE(b.ptr, nullptr);

    // Так как блоки лежат в абсолютно разных чанках ОС, их адреса не должны быть смежными
    EXPECT_NE(static_cast<std::byte*>(a.ptr) + 32, static_cast<std::byte*>(b.ptr));

    // 3. Проверяем reset — он должен корректно вернуть всю память без утечек
    arena.reset();
    
    // После сброса новая аллокация снова должна отработать штатно
    MemCore::Block c = arena.allocate(16, 8);
    EXPECT_NE(c.ptr, nullptr);
}