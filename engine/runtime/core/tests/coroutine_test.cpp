#include <tempest/coroutine.hpp>

#include <gtest/gtest.h>

TEST(coroutine, noop_coroutine_handle)
{
    auto handle = tempest::noop_coroutine();
    EXPECT_FALSE(handle.done());
    handle.resume();
    EXPECT_FALSE(handle.done());
    handle.destroy();
}

TEST(coroutine, suspend_always)
{
    struct Task
    {
        struct promise_type
        {
            auto get_return_object() -> Task
            {
                return {
                    .coro = handle_type::from_promise(*this),
                };
            }

            auto initial_suspend() -> tempest::suspend_always
            {
                return {};
            }

            auto final_suspend() noexcept -> tempest::suspend_always
            {
                return {};
            }

            void return_void()
            {
            }

            void unhandled_exception()
            {
            }
        };

        using handle_type = tempest::coroutine_handle<promise_type>;
        handle_type coro;
    };

    auto coro_func = []() -> Task { co_await tempest::suspend_always{}; };

    auto task = coro_func();
    auto handle = tempest::coroutine_handle<>::from_address(task.coro.address());
    EXPECT_FALSE(handle.done());

    handle.resume();
    EXPECT_FALSE(handle.done());

    handle.resume();
    EXPECT_TRUE(handle.done());

    handle.destroy();
}

TEST(coroutine, suspend_never)
{
    struct Task
    {
        struct promise_type
        {
            auto get_return_object() -> Task
            {
                return {
                    .coro = handle_type::from_promise(*this),
                };
            }

            auto initial_suspend() -> tempest::suspend_never
            {
                return {};
            }

            auto final_suspend() noexcept -> tempest::suspend_always
            {
                return {};
            }

            void return_void()
            {
            }
            
            void unhandled_exception()
            {
            }
        };

        using handle_type = tempest::coroutine_handle<promise_type>;
        handle_type coro;
    };

    auto coro_func = []() -> Task { co_return; };

    auto task = coro_func();
    auto handle = tempest::coroutine_handle<>::from_address(task.coro.address());
    EXPECT_TRUE(handle.done());

    handle.destroy();
}
