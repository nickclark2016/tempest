#include <gtest/gtest.h>

#include <tempest/job/async_event.hpp>
#include <tempest/job/task.hpp>
#include <tempest/job/types.hpp>
#include <tempest/profiler/session.hpp>

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

    // =========================================================================
    // SECTION: Monadic Chaining Tests
    // =========================================================================

    /// @brief Verifies that fallible task results can be transformed and chained
    ///        using expected's monadic operators (.transform, .and_then, .or_else).
    TEST(task_test, monadic_chaining_on_task_result)
    {
        // 1. Setup
        auto t = simple_success_task();

        // 2. Act
        t.resume();
        auto transformed = t.result()
                               .transform([](int x) { return x * 2; })
                               .and_then([](int x) -> expected<int, job_error> { return x + 10; });

        // 3. Assert
        EXPECT_TRUE(transformed.has_value());
        EXPECT_EQ(transformed.value(), 94);
    }

    /// @brief Verifies that monadic operations chain correctly inside a coroutine body.
    TEST(task_test, monadic_chaining_inside_coroutine)
    {
        // 1. Setup
        auto coroutine_monadic = []() -> task<expected<int, job_error>> {
            auto step = []() -> expected<int, job_error> { return 20; };
            auto res = step()
                           .transform([](int x) { return x * 2; })
                           .and_then([](int x) -> expected<int, job_error> { return x + 5; });
            co_return res;
        };

        // 2. Act
        auto t = coroutine_monadic();
        t.resume();

        // 3. Assert
        EXPECT_TRUE(t.is_ready());
        EXPECT_TRUE(t.has_value());
        auto inner = t.value();
        EXPECT_TRUE(inner.has_value());
        EXPECT_EQ(inner.value(), 45);
    }

    /// @brief Verifies that an error in a monadic chain is propagated and skips downstream transforms.
    TEST(task_test, monadic_chaining_error_propagation_inside_coroutine)
    {
        // 1. Setup
        auto coroutine_fail = []() -> task<expected<int, job_error>> {
            auto step = []() -> expected<int, job_error> { return unexpected(job_error::timeout); };
            auto res = step()
                           .transform([](int x) { return x * 2; })
                           .and_then([](int x) -> expected<int, job_error> { return x + 5; });
            co_return res;
        };

        // 2. Act
        auto t = coroutine_fail();
        t.resume();

        // 3. Assert
        EXPECT_TRUE(t.is_ready());
        EXPECT_TRUE(t.has_value());
        auto inner = t.value();
        EXPECT_FALSE(inner.has_value());
        EXPECT_EQ(inner.error(), job_error::timeout);
    }

    // =========================================================================
    // SECTION: Task Continuation (.then) Tests
    // =========================================================================

    /// @brief Verifies that task::then chains a synchronous value transformation
    ///        producing a new task with the transformed value.
    TEST(task_test, continuation_value_transform)
    {
        // 1. Setup
        auto t = simple_success_task();

        // 2. Act
        auto cont = t.then([](int x) { return x * 2; });
        const auto finished = cont.resume();

        // 3. Assert
        EXPECT_TRUE(finished);
        EXPECT_TRUE(cont.is_ready());
        EXPECT_TRUE(cont.has_value());
        EXPECT_EQ(cont.value(), 84);
    }

    /// @brief Verifies that task::then chains an asynchronous continuation returning
    ///        another coroutine task and unwraps the nested task result seamlessly.
    TEST(task_test, continuation_async_task)
    {
        // 1. Setup
        auto t = simple_success_task();

        // 2. Act
        auto cont = t.then([](int x) -> task<int> { co_return x + 100; });
        const auto finished = cont.resume();

        // 3. Assert
        EXPECT_TRUE(finished);
        EXPECT_TRUE(cont.is_ready());
        EXPECT_TRUE(cont.has_value());
        EXPECT_EQ(cont.value(), 142);
    }

    /// @brief Verifies that task::then with a void-returning callable executes correctly
    ///        and completes as task<void, E>.
    TEST(task_test, continuation_void_return)
    {
        // 1. Setup
        auto t = simple_success_task();
        bool executed = false;

        // 2. Act
        auto cont = t.then([&](int x) {
            EXPECT_EQ(x, 42);
            executed = true;
        });
        const auto finished = cont.resume();

        // 3. Assert
        EXPECT_TRUE(finished);
        EXPECT_TRUE(executed);
        EXPECT_TRUE(cont.is_ready());
        EXPECT_TRUE(cont.has_value());
    }

    /// @brief Verifies that task::then on a task<void> correctly invokes a callable
    ///        with no arguments and returns a valued task.
    TEST(task_test, continuation_on_void_task)
    {
        // 1. Setup
        bool executed1 = false;
        auto t = simple_void_task(&executed1);

        // 2. Act
        auto cont = t.then([]() { return 777; });
        const auto finished = cont.resume();

        // 3. Assert
        EXPECT_TRUE(finished);
        EXPECT_TRUE(executed1);
        EXPECT_TRUE(cont.is_ready());
        EXPECT_TRUE(cont.has_value());
        EXPECT_EQ(cont.value(), 777);
    }

    /// @brief Verifies that task::then on a task<void> with a void callable completes cleanly.
    TEST(task_test, continuation_on_void_task_to_void)
    {
        // 1. Setup
        bool executed1 = false;
        bool executed2 = false;
        auto t = simple_void_task(&executed1);

        // 2. Act
        auto cont = t.then([&]() { executed2 = true; });
        const auto finished = cont.resume();

        // 3. Assert
        EXPECT_TRUE(finished);
        EXPECT_TRUE(executed1);
        EXPECT_TRUE(executed2);
        EXPECT_TRUE(cont.is_ready());
        EXPECT_TRUE(cont.has_value());
    }

    /// @brief Verifies that multiple .then() continuations can be chained in sequence.
    TEST(task_test, continuation_multi_chain)
    {
        // 1. Setup
        auto t = simple_success_task();

        // 2. Act: (42 + 8) * 2 - 10 = 90
        auto chained = t.then([](int x) { return x + 8; })
                           .then([](int x) { return x * 2; })
                           .then([](int x) { return x - 10; });
        const auto finished = chained.resume();

        // 3. Assert
        EXPECT_TRUE(finished);
        EXPECT_TRUE(chained.is_ready());
        EXPECT_TRUE(chained.has_value());
        EXPECT_EQ(chained.value(), 90);
    }

    /// @brief Verifies that when the antecedent task fails, the continuation lambda is
    ///        never invoked and the error propagates to the resulting task.
    TEST(task_test, continuation_error_short_circuit)
    {
        // 1. Setup
        auto t = child_fail_task();
        bool continuation_called = false;

        // 2. Act
        auto cont = t.then([&](int x) {
            continuation_called = true;
            return x * 2;
        });
        const auto finished = cont.resume();

        // 3. Assert
        EXPECT_TRUE(finished);
        EXPECT_TRUE(cont.is_ready());
        EXPECT_FALSE(cont.has_value());
        EXPECT_EQ(cont.error(), job_error::canceled);
        EXPECT_FALSE(continuation_called);
    }

    /// @brief Verifies that non-fallible task<T, void> supports .then() continuation.
    TEST(task_test, continuation_non_fallible_task)
    {
        // 1. Setup
        auto t = non_fallible_task();

        // 2. Act
        auto cont = t.then([](int x) { return x + 1; });
        const auto finished = cont.resume();

        // 3. Assert
        EXPECT_TRUE(finished);
        EXPECT_TRUE(cont.is_ready());
        EXPECT_EQ(cont.value(), 100);
    }

    /// @brief Verifies that non-fallible task<void, void> supports .then() continuation.
    TEST(task_test, continuation_non_fallible_void_task)
    {
        // 1. Setup
        bool step1 = false;
        bool step2 = false;
        auto t = non_fallible_void_task(&step1);

        // 2. Act
        auto cont = t.then([&]() { step2 = true; });
        const auto finished = cont.resume();

        // 3. Assert
        EXPECT_TRUE(finished);
        EXPECT_TRUE(step1);
        EXPECT_TRUE(step2);
        EXPECT_TRUE(cont.is_ready());
    }

    /// @brief Verifies that a coroutine executing with an active profiler session
    ///        records execution slices with matching coroutine ID, incrementing slice indices,
    ///        and propagated suspend reasons.
    TEST(task_test, coroutine_profiler_slice_tracking)
    {
        // 1. Setup
        auto prof = profiler::profiler_session{true};
        [[maybe_unused]] auto& ctx = prof.get_or_register_thread();

        auto evt = async_event{false};
        auto resumed = false;

        auto test_coroutine = [&]() -> task<int> {
            co_await evt;
            resumed = true;
            co_return 42;
        };

        // 2. Act: Start coroutine (slice 0 runs until co_await evt suspends)
        auto t = test_coroutine();
        const auto coro_id = t.coroutine_id();
        EXPECT_GT(coro_id, 0U);

        const auto first_step = t.resume();
        EXPECT_FALSE(first_step);
        EXPECT_FALSE(resumed);

        // Resume coroutine by signaling event and resuming task
        evt.set();
        const auto second_step = t.resume();
        EXPECT_TRUE(second_step);
        EXPECT_TRUE(resumed);
        EXPECT_EQ(t.value(), 42);

        // 3. Assert: Verify profiler captured 2 slices with matching coroutine_id
        auto chunks = prof.drain_completed_chunks();
        ASSERT_FALSE(chunks.empty());

        auto coro_zones = vector<profiler::zone_record>{};
        for (const auto& chunk : chunks)
        {
            for (const auto& z : chunk->zones())
            {
                if (z.coroutine_id == coro_id)
                {
                    coro_zones.push_back(z);
                }
            }
        }

        ASSERT_EQ(coro_zones.size(), 2U);

        // Slice 0 suspended due to event_wait
        EXPECT_EQ(coro_zones[0].slice_index, 0U);
        EXPECT_EQ(coro_zones[0].reason, profiler::suspend_reason::event_wait);

        // Slice 1 completed
        EXPECT_EQ(coro_zones[1].slice_index, 1U);
        EXPECT_EQ(coro_zones[1].reason, profiler::suspend_reason::completed);

        profiler::thread_profiler_context::set_current_thread_context(nullptr);
    }

    /// @brief Verify that task naming via with_name() and set_name() updates task name and profiler slice zone names
    TEST(task_test, task_custom_naming_fluent)
    {
        // 1. Setup profiler
        auto prof = profiler::profiler_session{true};
        auto& ctx = prof.get_or_register_thread();
        profiler::thread_profiler_context::set_current_thread_context(&ctx);

        auto test_coro = []() -> task<int> {
            co_return 99;
        };

        // 2. Act: Name coroutine fluently before running
        auto t = test_coro().with_name("MyCustomRenderPass");
        EXPECT_EQ(t.name(), "MyCustomRenderPass");
        const auto coro_id = t.coroutine_id();

        const auto completed = t.resume();
        EXPECT_TRUE(completed);
        EXPECT_EQ(t.value(), 99);

        // 3. Assert: Verify profiler slice zone has custom name
        auto chunks = prof.drain_completed_chunks();
        ASSERT_FALSE(chunks.empty());

        auto found_custom_name = false;
        for (const auto& chunk : chunks)
        {
            for (const auto& z : chunk->zones())
            {
                if (z.coroutine_id == coro_id && z.name == "MyCustomRenderPass")
                {
                    found_custom_name = true;
                }
            }
        }
        EXPECT_TRUE(found_custom_name);

        profiler::thread_profiler_context::set_current_thread_context(nullptr);
    }

    /// @brief Verify that co_await set_task_name dynamically renames the task and active profiler slice
    TEST(task_test, task_custom_naming_co_await_set_task_name)
    {
        // 1. Setup profiler
        auto prof = profiler::profiler_session{true};
        auto& ctx = prof.get_or_register_thread();
        profiler::thread_profiler_context::set_current_thread_context(&ctx);

        auto test_coro = []() -> task<void> {
            co_await set_task_name{"DynamicPassName"};
            co_return;
        };

        // 2. Act: Run coroutine that self-renames
        auto t = test_coro();
        const auto coro_id = t.coroutine_id();

        const auto completed = t.resume();
        EXPECT_TRUE(completed);
        EXPECT_EQ(t.name(), "DynamicPassName");

        // 3. Assert: Verify profiler recorded zone with DynamicPassName
        auto chunks = prof.drain_completed_chunks();
        ASSERT_FALSE(chunks.empty());

        auto found_dynamic_name = false;
        for (const auto& chunk : chunks)
        {
            for (const auto& z : chunk->zones())
            {
                if (z.coroutine_id == coro_id && z.name == "DynamicPassName")
                {
                    found_dynamic_name = true;
                }
            }
        }
        EXPECT_TRUE(found_dynamic_name);

        profiler::thread_profiler_context::set_current_thread_context(nullptr);
    }
} // namespace tempest::job::tests
