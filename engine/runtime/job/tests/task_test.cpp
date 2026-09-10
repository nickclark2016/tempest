#include <gtest/gtest.h>

#include <tempest/job/task.hpp>
#include <tempest/job/types.hpp>

namespace tempest::job::tests
{
    namespace
    {
        struct raii_tracker
        {
            int* counter{nullptr};

            explicit raii_tracker(int* c) noexcept : counter{c}
            {
            }

            ~raii_tracker()
            {
                if (counter != nullptr)
                {
                    ++(*counter);
                }
            }

            raii_tracker(const raii_tracker&) = delete;
            raii_tracker& operator=(const raii_tracker&) = delete;
            raii_tracker(raii_tracker&& other) noexcept : counter{other.counter}
            {
                other.counter = nullptr;
            }
            raii_tracker& operator=(raii_tracker&& other) noexcept
            {
                if (this != &other)
                {
                    if (counter != nullptr)
                    {
                        ++(*counter);
                    }
                    counter = other.counter;
                    other.counter = nullptr;
                }
                return *this;
            }
        };

        auto simple_success_task() -> task<int>
        {
            co_return 42;
        }

        auto simple_void_task(bool* executed) -> task<void>
        {
            *executed = true;
            co_return;
        }

        auto non_fallible_task() -> task<int, void>
        {
            co_return 99;
        }

        auto non_fallible_void_task(bool* executed) -> task<void, void>
        {
            *executed = true;
            co_return;
        }

        auto child_fail_task() -> task<int>
        {
            co_await unexpected(job_error::canceled);
            co_return 10;
        }

        auto parent_short_circuit_task(int* parent_dtor, int* executed_after) -> task<int>
        {
            auto tracker = raii_tracker{parent_dtor};
            auto val = co_await child_fail_task();
            *executed_after = val;
            co_return val * 2;
        }
    } // namespace

    // =========================================================================
    // SECTION: Task Success & Value Unwrapping Tests
    // =========================================================================

    /// @brief Verifies that a fallible task returning an integer completes successfully
    ///        and delivers its expected value upon direct resumption.
    TEST(task_test, success_value_unwrapping)
    {
        // 1. Setup
        auto t = simple_success_task();

        // 2. Act
        const auto finished = t.resume();

        // 3. Assert
        EXPECT_TRUE(finished);
        EXPECT_TRUE(t.is_ready());
        EXPECT_TRUE(t.has_value());
        EXPECT_EQ(t.value(), 42);
        EXPECT_EQ(t.error(), job_error::none);
    }

    /// @brief Verifies that a fallible task returning void completes successfully
    ///        and marks its expected value as valid.
    TEST(task_test, success_void_task)
    {
        // 1. Setup
        bool executed = false;
        auto t = simple_void_task(&executed);

        // 2. Act
        const auto finished = t.resume();

        // 3. Assert
        EXPECT_TRUE(finished);
        EXPECT_TRUE(executed);
        EXPECT_TRUE(t.is_ready());
        EXPECT_TRUE(t.has_value());
    }

    /// @brief Verifies that non-fallible task<T, void> and task<void, void> execute
    ///        cleanly without error overhead.
    TEST(task_test, non_fallible_tasks)
    {
        // 1. Setup
        auto t1 = non_fallible_task();
        bool executed = false;
        auto t2 = non_fallible_void_task(&executed);

        // 2. Act
        t1.resume();
        t2.resume();

        // 3. Assert
        EXPECT_TRUE(t1.is_ready());
        EXPECT_EQ(t1.value(), 99);
        EXPECT_TRUE(t2.is_ready());
        EXPECT_TRUE(executed);
    }

    // =========================================================================
    // SECTION: Error Short-Circuiting & RAII Frame Destruction Tests
    // =========================================================================

    /// @brief Verifies that awaiting an unexpected error terminates the coroutine
    ///        immediately and sets the task error code.
    TEST(task_test, direct_unexpected_short_circuit)
    {
        // 1. Setup
        auto t = child_fail_task();

        // 2. Act
        t.resume();

        // 3. Assert
        EXPECT_TRUE(t.is_ready());
        EXPECT_FALSE(t.has_value());
        EXPECT_EQ(t.error(), job_error::canceled);
    }

    /// @brief Verifies that when a child task fails with an error, the parent task
    ///        propagates the error, skips subsequent statements, and runs destructors
    ///        for all in-scope local frame variables.
    TEST(task_test, parent_short_circuit_and_raii_unwinding)
    {
        // 1. Setup
        int parent_dtor_count = 0;
        int executed_after = 0;
        auto parent = parent_short_circuit_task(&parent_dtor_count, &executed_after);

        // 2. Act
        parent.resume();

        // 3. Assert
        EXPECT_TRUE(parent.is_ready());
        EXPECT_FALSE(parent.has_value());
        EXPECT_EQ(parent.error(), job_error::canceled);
        EXPECT_EQ(executed_after, 0);       // Code after co_await was never reached
        EXPECT_EQ(parent_dtor_count, 1);    // RAII destructor fired cleanly during short-circuit
    }
} // namespace tempest::job::tests
