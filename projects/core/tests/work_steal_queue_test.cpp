#include <gtest/gtest.h>

#include <tempest/threading/work_steal_queue.hpp>

#include <thread>
#include <vector>

using tempest::core::threading::task_priority;
using tempest::core::threading::work_steal_queue;

namespace
{
    void threaded_work_steal(std::size_t thread_count)
    {
        for (std::size_t test_cap = 1; test_cap < 4096; test_cap = 2 * test_cap + 1)
        {
            work_steal_queue<void*> wsq;
            std::vector<void*> data(test_cap);
            
            std::atomic<std::size_t> idx{0};

            for (std::size_t i = 0; i < test_cap; ++i)
            {
                data[i] = &i;
            }

            // create thief threads
            std::vector<std::thread> thieves;
            std::vector<std::vector<void*>> stolen_values(thread_count);

            for (std::size_t i = 0; i < thread_count; ++i)
            {
                // capture i by value so it's still good
                thieves.emplace_back([&, i]() {
                    while (idx != test_cap)
                    {
                        auto ptr = wsq.steal();
                        if (ptr)
                        {
                            stolen_values[i].push_back(ptr);
                            idx.fetch_add(1, std::memory_order_relaxed); // no need to reorder here
                        }
                    }
                });
            }

            for (std::size_t i = 0; i < test_cap; ++i)
            {
                wsq.push(data[i], task_priority::HIGH);
            }

            // on the main thread, pop while the thiefs are stealing, emulating work steal in a contended environment
            std::vector<void*> results;
            while (idx != test_cap)
            {
                auto ptr = wsq.pop();
                if (ptr)
                {
                    results.push_back(ptr);
                    idx.fetch_add(1, std::memory_order_relaxed);
                }
            }

            ASSERT_TRUE(wsq.empty()); // check if we're empty
            ASSERT_EQ(wsq.steal(), nullptr); // sanity check
            ASSERT_EQ(wsq.pop(), nullptr); // sanity check

            for (auto& thief : thieves)
            {
                thief.join();
            }

            for (auto& local_stolen : stolen_values)
            {
                for (auto stolen : local_stolen)
                {
                    results.push_back(stolen);
                }
            }

            // sort for easy compare
            std::sort(results.begin(), results.end());
            std::sort(data.begin(), data.end());

            ASSERT_EQ(results.size(), test_cap);
            ASSERT_EQ(results, data);
        }
    }
}

TEST(work_steal_queue, DefaultConstructor)
{
    std::uint64_t capacity = 2;
    work_steal_queue<int*> wsq{capacity};

    ASSERT_TRUE(wsq.empty());
    ASSERT_EQ(wsq.size(), 0);
    ASSERT_GE(wsq.capacity(), 2); // at least 2, as it's 2 per priority queue
}

TEST(work_steal_queue, OwnerPushPop)
{
    for (std::size_t test_cap = 1; test_cap < 4096; test_cap = 2 * test_cap + 1) // semi random sizes
    {
        work_steal_queue<void*> wsq;
        std::vector<void*> data(test_cap);
        
        ASSERT_TRUE(wsq.empty());

        for (std::size_t i = 0; i < test_cap; ++i)
        {
            data[i] = &i; // dangling, we'll never read though
            wsq.push(data[i], task_priority::HIGH);
        }

        for (std::size_t i = 0; i < test_cap; ++i)
        {
            auto ptr = wsq.pop();
            ASSERT_NE(ptr, nullptr);
            ASSERT_EQ(data[test_cap - i - 1], ptr);
        }

        // Make sure we pop a nullptr now that we're empty
        ASSERT_EQ(wsq.pop(), nullptr);
        ASSERT_TRUE(wsq.empty());
    }
}

TEST(work_steal_queue, OwnerPushSteal)
{
    for (std::size_t test_cap = 1; test_cap < 4096; test_cap = 2 * test_cap + 1) // semi random sizes
    {
        work_steal_queue<void*> wsq;
        std::vector<void*> data(test_cap);

        ASSERT_TRUE(wsq.empty());

        for (std::size_t i = 0; i < test_cap; ++i)
        {
            data[i] = &i; // dangling, we'll never read though
            wsq.push(data[i], task_priority::HIGH);
        }

        for (std::size_t i = 0; i < test_cap; ++i)
        {
            auto ptr = wsq.steal();
            ASSERT_NE(ptr, nullptr);
            ASSERT_EQ(data[test_cap - i - 1], ptr);
        }

        // Make sure we pop a nullptr now that we're empty
        ASSERT_EQ(wsq.steal(), nullptr);
        ASSERT_TRUE(wsq.empty());
    }
}

TEST(work_steal_queue, MultiThreadedSteal)
{
    threaded_work_steal(1);
    threaded_work_steal(2);
    threaded_work_steal(3);
    threaded_work_steal(4);
    threaded_work_steal(8);
    threaded_work_steal(16);
    threaded_work_steal(32);
}