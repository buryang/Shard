#include "gtest/gtest.h"
#include "Utils/Memory.h"
#include <chrono>
#include <thread>

using namespace Shard;
using namespace Shard::Utils;

TEST(TEST_MEM_ALLOCATOR, TEST_SCALABLE_POOL_ALLOCATOR)
{
    auto scalable_allocator = TLSScalablePoolAllocatorInstance<uint8_t, 0>();
    ASSERT_NE(scalable_allocator, nullptr);
    for (auto n = 12; n < 4 * 1024 * 1024; n *= 4)
    {
        auto* raw_ptr0 = scalable_allocator->allocate(n);
        ASSERT_NE(raw_ptr0, nullptr);
        auto* raw_ptr1 = scalable_allocator->allocate(n);
        auto del_thread = std::thread([&]() { scalable_allocator->deallocate(raw_ptr1, n); });
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        del_thread.join();
        scalable_allocator->deallocate(raw_ptr0, n);
    }

    {
        //test allocator same size blocks
        Vector<uintptr_t>   alloc_result;
        auto block_allocator = TLSScalablePoolAllocatorInstance<uint8_t, 1>();
        for (auto n = 0; n < 10000; ++n)
        {
            auto* temp = block_allocator->allocate(100);
            ASSERT_NE(temp, nullptr);
            alloc_result.emplace_back((uintptr_t)temp);
        }
        for (auto ptr : alloc_result)
        {
            block_allocator->deallocate((uint8_t*)ptr, 100);
        }
    }
}

#include "Utils/Memory.cpp"