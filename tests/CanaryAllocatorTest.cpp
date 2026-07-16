#include <gtest/gtest.h>

#include <MemCore/CanaryAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>


TEST(CanaryAllocatorTest, NormalUseDoesNotCrash) 
{
    MemCore::MallocUpstream malloc_up;
    MemCore::CanaryAllocator<MemCore::MallocUpstream> canary(malloc_up);

    MemCore::Block b = canary.allocate(10, 8);
    ASSERT_NE(b.ptr, nullptr);
    
    // Записываем ровно 10 байт. Всё легально.
    std::memset(b.ptr, 0xAA, 10);
    
    // Должно пройти успешно, без крашей
    canary.deallocate(b.ptr, b.size);
}

TEST(CanaryAllocatorTest, DetectsBufferOverflow) 
{
    MemCore::MallocUpstream malloc_up;
    MemCore::CanaryAllocator<MemCore::MallocUpstream> canary(malloc_up);

    // Ожидаем, что код внутри ASSERT_DEATH убьет программу с сообщением "Back Canary corrupted"
    ASSERT_DEATH(
        {
            MemCore::Block b = canary.allocate(10, 8);
            
            // НАМЕРЕННЫЙ БАГ: Записываем 11 байт в массив размером 10!
            // Это затрет один байт задней канарейки.
            std::memset(b.ptr, 0xAA, 11);
            
            canary.deallocate(b.ptr, b.size);
        }, "Back Canary corrupted");
}

TEST(CanaryAllocatorTest, DetectsBufferUnderflow) 
{
    MemCore::MallocUpstream malloc_up;
    MemCore::CanaryAllocator<MemCore::MallocUpstream> canary(malloc_up);

    // Ожидаем смерть от "Front Canary corrupted"
    ASSERT_DEATH(
        {
            MemCore::Block b = canary.allocate(10, 8);
            
            // НАМЕРЕННЫЙ БАГ: Пишем по отрицательному индексу (до начала массива)
            std::byte* payload = static_cast<std::byte*>(b.ptr);
            payload[-1] = std::byte{0xFF};
            
            canary.deallocate(b.ptr, b.size);
        }, "Front Canary corrupted");
}