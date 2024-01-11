#include "gtest/gtest.h"
#include "Utils/CommonUtils.h"
#include "Utils/SimpleCoro.h"


using namespace Shard;
using namespace Shard::Utils;

TEST(TEST_JOB_SYSTEM, TEST_CORO_TASK)
{
    SimpleJobSystem::Instance().Init(std::thread::hardware_concurrency(), 16);
    auto hello_world = []()->Coro<>
    {
        std::cerr << "hello world! Coro" << std::endl;
        co_return;
    };
    Schedule(hello_world(), nullptr, 0xFFFFFFFF, false);

    auto counter_coro = []()->Coro<uint32_t>
    {
        uint32_t counter = 20u;
        while (counter-- > 0)
        {
            co_yield counter;
        }
    };
    Schedule(counter_coro(), nullptr, 0xFFFFFFFF, false);

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
    Schedule(hello_world_number(), nullptr, 0xFFFFFFFF, false);

    //test schedule seq
    {
        std::atomic_bool flag{ false };
        auto counter = std::make_shared<uint32_t>(0u);
        auto sequential_number = [counter, &flag]()->Coro<> {
            auto inc_number = [counter]()->Coro<> {
                (*counter)++;
                co_return;
            };
            co_await AwaitTupleHelper::Sequential(inc_number(), inc_number(), inc_number());
            flag.store(true, std::memory_order::release);
        };
        Schedule(sequential_number());
        while (!flag.load(std::memory_order::acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        EXPECT_EQ(*counter, 3);
    }
    //test schedule parallel
    {
        std::atomic_bool flag{ false };
        std::atomic_int32_t counter{ 0u };
        auto parallel_number = [&]()->Coro<> {
            auto inc_number = [&counter]()->Coro<> {
                std::cerr << "hello world! " << std::this_thread::get_id() << std::endl;
                counter.fetch_add(1u, std::memory_order::acq_rel);
                co_return;
            };
            co_await AwaitTupleHelper::Parallel(inc_number(), inc_number(), inc_number(), inc_number(), inc_number());
            flag.store(true, std::memory_order::release);
        };
        Schedule(parallel_number());
        while (!flag.load(std::memory_order::acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        EXPECT_EQ(counter.load(), 5);
    }
    
}

TEST(TEST_JOB_SYSTEM, TEST_FUNC_TASK)
{
    auto hello_world = []() {
        std::cerr << "hello world!, function" << std::endl;
    };
    std::atomic_bool flag{ false };
    Schedule(hello_world, nullptr, 0xFFFFFFFFF, false);

    auto cycle_print = [&flag]()->void {
        uint32_t counter = 0u;
        while (counter++ < 100u) {
            if (counter % 10 == 0) {
                std::cerr << "cycle hello world! " << counter << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        flag.store(true, std::memory_order::release);
    };
    Schedule(cycle_print, nullptr, 0xFFFFFFFF, true);

    while (!flag.load(std::memory_order::acquire))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_EQ(flag.load(), true);
}

TEST(TEST_JOB_SYSTEM, TEST_MIX_TASK)
{
    auto hello_world_coro = []()->Coro<>
    {
        std::cerr << "hello world! Coro" << std::endl;
        co_return;
    };
    
    Schedule(hello_world_coro());

    auto hello_world_func = []() {
        std::cerr << "hello world!, function" << std::endl;
    };
    
    Schedule(hello_world_func, nullptr, 0x1, false);

    auto mix_hello_world = [=]()->Coro<>{
        co_await AwaitTupleHelper::Parallel(hello_world_func, hello_world_coro());
    };
    Schedule(mix_hello_world(), nullptr);

    //test continuation
    auto hello_world_func2 = [hello_world_coro]()
    {
        std::cerr << "hello world again! Coro" << std::endl;
        Continuation(hello_world_coro());
    };
    Schedule(hello_world_func2, nullptr, 0x1, false);
    SimpleJobSystem::Instance().UnInit();
}

