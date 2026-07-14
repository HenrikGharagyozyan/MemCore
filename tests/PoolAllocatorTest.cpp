#include <gtest/gtest.h>
#include <MemCore/PoolAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <vector>

class PoolAllocatorTest : public ::testing::Test 
{
protected:
    MemCore::MallocUpstream upstream;
    MemCore::Block chunk;

    void SetUp() override 
    {
        chunk = upstream.allocate(1024, 8); // Выделяем 1 КБ под пул
    }

    void TearDown() override 
    {
        upstream.deallocate(chunk.ptr, chunk.size);
    }
};

TEST_F(PoolAllocatorTest, AllocateAndDeallocateAnyOrder) 
{
    // Создаем пул для чанков по 32 байта с выравниванием 8
    MemCore::PoolAllocator pool(chunk, 32, 8);

    MemCore::Block a = pool.allocate(32, 8);
    MemCore::Block b = pool.allocate(32, 8);
    MemCore::Block c = pool.allocate(32, 8);

    EXPECT_NE(a.ptr, nullptr);
    EXPECT_NE(b.ptr, nullptr);
    EXPECT_NE(c.ptr, nullptr);

    // Удаляем из "серединки" (Блок B)
    pool.deallocate(b.ptr, b.size);

    // Следующая аллокация обязана мгновенно переиспользовать место блока B
    MemCore::Block d = pool.allocate(32, 8);
    EXPECT_EQ(d.ptr, b.ptr);
}

TEST_F(PoolAllocatorTest, OutOfMemoryHandling) 
{
    // 1024 байта / 128 байт = ровно 8 чанков
    MemCore::PoolAllocator pool(chunk, 128, 8);

    std::vector<MemCore::Block> blocks;
    // Вычерпываем весь пул
    for (int i = 0; i < 8; ++i) 
    {
        blocks.push_back(pool.allocate(128, 8));
        EXPECT_NE(blocks.back().ptr, nullptr);
    }

    // 9-я попытка должна вернуть nullptr (пул пуст)
    MemCore::Block overflow = pool.allocate(128, 8);
    EXPECT_EQ(overflow.ptr, nullptr);
}