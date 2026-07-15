#include <gtest/gtest.h>

#include <MemCore/MemoryHelpers.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>

#include <string>

// Global counter for verifying destructors
static int g_destructor_calls = 0;

// Test class with a constructor and a destructor
class TestEntity 
{
public:
    int id;
    std::string name;

    TestEntity(int i, std::string n) 
        : id(i), name(std::move(n)) 
    {
        // Constructor completed
    }

    ~TestEntity() 
    {
        g_destructor_calls++;
    }
};

TEST(MemoryHelpersTest, NewAndDestroysCorrectly) 
{
    g_destructor_calls = 0;
    MemCore::MallocUpstream system_malloc;
    MemCore::Block chunk = system_malloc.allocate(1024, 8);
    
    {
        MemCore::LinearAllocator linear(chunk);

        // 1. Create an object with MemCore::New
        // Pass arguments (10, "Player") just like std::make_shared
        TestEntity* entity = MemCore::New<TestEntity>(linear, 10, "Player");

        // 2. Verify that the object was actually created and the constructor ran
        ASSERT_NE(entity, nullptr);
        EXPECT_EQ(entity->id, 10);
        EXPECT_EQ(entity->name, "Player");

        // 3. Delete the object
        MemCore::Delete(linear, entity);

        // 4. Verify that the destructor was called exactly once
        EXPECT_EQ(g_destructor_calls, 1);
    }

    system_malloc.deallocate(chunk.ptr, chunk.size);
}