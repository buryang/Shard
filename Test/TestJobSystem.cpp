#include "gtest/gtest.h"
#include "Utils/CommonUtils.h"
#include "Utils/SimpleCoro.h"


using namespace Shard;
using namespace Shard::Utils;

TEST(TEST_JOB_SYSTEM, TEST_CORO_TASK)
{
    auto hello_world = []()->Coro<>
    {
        std::cerr << "hello world! Coro" << std::endl;
        co_return;
    };
    Schedule(hello_world);

    auto counter_coro = []()->Coro<uint32_t>
    {
        uint32_t counter = 20u;
        while (counter-- > 0)
        {
            co_yield counter;
        }
        co_return counter;
    };
    Schedule(counter_coro);

    auto hello_world_number = [counter_coro]()->Coro<>
    {
        auto counter = counter_coro();
        for (auto n = 0; n < 10u; ++n)
        {
            uint32_t number = co_await counter;
            std::cerr << fmt::format("hello world! {}", number) << std::endl;
        }
        co_return;
    };
    Schedule(hello_world_number);

    //test schedule seq
    {
        uint32_t counter = 0u;
        auto sequential_number = [&counter]()->Coro<> {
            auto inc_number = [&counter]()->Coro<> {
                counter++;
                co_return;
            };
            co_await AwaitTupleHelper::Sequential(inc_number, inc_number, inc_number);
        };
        auto seq_job = Schedule(sequential_number);
        while (!seq_job->IsFinished()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        ASSERT_EQ(counter, 3);
    }

    //test schedule parallel
    
    {
        std::atomic_int counter = 0;
        auto parallel_number = [&counter]()->Coro<> {
            auto inc_number = [&counter]()->Coro<> {
                auto curr_counter = counter.fetch_add(1);
                std::cerr << "hello world! " << curr_counter << std::endl;
                co_return;
            };
            co_await AwaitTupleHelper::Parallel(inc_number, inc_number, inc_number);
        };
        Schedule(parallel_number);
    }
    
}

TEST(TEST_JOB_SYSTEM, TEST_FUNC_TASK)
{
    auto hello_world = []() {
        std::cerr << "hello world!, function";
    };
    auto hello_handle = Schedule(hello_world, nullptr, 0x1, false);
    ASSERT_TRUE(hello_handle != nullptr);

    auto cycle_print = []()->void {
        uint32_t counter = 0u;
        while (counter < 10000) {
            std::cerr << "cycle hello world! " << counter << std::endl;
        }
    };
    auto cycle_handle = Schedule(cycle_print);
    ASSERT_TRUE(cycle_handle != nullptr);

    std::cerr << fmt::format("hello world task state: {}", hello_handle->IsFinished()) << std::endl;
    std::cerr << fmt::format("cycle world task state: {}", cycle_handle->IsFinished()) << std::endl;
}

TEST(TEST_JOB_SYSTEM, TEST_MIX_TASK)
{
    auto hello_world_coro = []()->Coro<>
    {
        std::cerr << "hello world! Coro" << std::endl;
        co_return;
    };
    auto coro_handle = Schedule(hello_world_coro);
    ASSERT_TRUE(coro_handle != nullptr);

    auto hello_world_func = []() {
        std::cerr << "hello world!, function";
    };
    auto func_handle = Schedule(hello_world_func, nullptr, 0x1, false);
    ASSERT_TRUE(func_handle != nullptr);

    //test continuation
    auto hello_world_coro2 = []()->Coro<>
    {
        std::cerr << "hello world again! Coro" << std::endl;
        co_return;
    };
}

#include "Utils/SimpleJobSystem.cpp"