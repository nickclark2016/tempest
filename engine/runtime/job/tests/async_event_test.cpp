#include <gtest/gtest.h>

#include <tempest/job/async_event.hpp>
#include <tempest/job/task.hpp>

namespace tempest::job::tests
{
    namespace
    {
        auto waiter_task(async_event& evt, bool* flag) -> task<void>
        {
            co_await evt.wait();
            *flag = true;
            co_return;
        }
    } // namespace

    // =========================================================================
    // SECTION: Manual-Reset Event Tests
    // =========================================================================

    /// @brief Verifies that setting a manual-reset event awakens all currently waiting
    ///        coroutines and allows subsequent waiters to pass without suspending until reset.
    TEST(async_event_test, manual_reset_broadcast)
    {
        // 1. Setup
        auto evt = async_event{false, event_reset_mode::manual};
        bool w1_ran = false;
        bool w2_ran = false;
        bool w3_ran = false;

        auto t1 = waiter_task(evt, &w1_ran);
        auto t2 = waiter_task(evt, &w2_ran);

        t1.resume();
        t2.resume();

        EXPECT_FALSE(w1_ran);
        EXPECT_FALSE(w2_ran);

        // 2. Act: Set event
        evt.set();

        // 3. Assert: Both waiters awakened
        EXPECT_TRUE(w1_ran);
        EXPECT_TRUE(w2_ran);
        EXPECT_TRUE(evt.is_set());

        // New waiter arrives while event is still set
        auto t3 = waiter_task(evt, &w3_ran);
        t3.resume();
        EXPECT_TRUE(w3_ran);

        // Reset clears signaled state
        evt.reset();
        EXPECT_FALSE(evt.is_set());
    }

    // =========================================================================
    // SECTION: Auto-Reset Event Tests
    // =========================================================================

    /// @brief Verifies that setting an auto-reset event awakens exactly one waiter,
    ///        leaving subsequent waiters suspended until the event is set again.
    TEST(async_event_test, auto_reset_single_wake)
    {
        // 1. Setup
        auto evt = async_event{false, event_reset_mode::auto_reset};
        bool w1_ran = false;
        bool w2_ran = false;

        auto t1 = waiter_task(evt, &w1_ran);
        auto t2 = waiter_task(evt, &w2_ran);

        t1.resume();
        t2.resume();

        EXPECT_FALSE(w1_ran);
        EXPECT_FALSE(w2_ran);

        // 2. Act: First set
        evt.set();

        // 3. Assert: Exactly one waiter should have run
        EXPECT_TRUE(w1_ran ^ w2_ran);
        EXPECT_FALSE(evt.is_set()); // Automatically reset

        // Second set awakens the remaining waiter
        evt.set();
        EXPECT_TRUE(w1_ran && w2_ran);
        EXPECT_FALSE(evt.is_set());
    }
} // namespace tempest::job::tests
