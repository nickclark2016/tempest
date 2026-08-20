#include <gtest/gtest.h>

#include <tempest/deque.hpp>
#include <tempest/iterator.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

#include <format>

namespace tempest
{
    TEST(iterator_test, back_inserter_with_vector)
    {
        auto vec = vector<int>{};
        auto it = back_inserter(vec);

        *it = 1;
        ++it;
        *it = 2;
        it++;
        *it = 3;

        ASSERT_EQ(vec.size(), 3U);
        EXPECT_EQ(vec[0], 1);
        EXPECT_EQ(vec[1], 2);
        EXPECT_EQ(vec[2], 3);
    }

    TEST(iterator_test, back_inserter_with_string)
    {
        auto str = string{};
        auto it = back_inserter(str);

        *it = 'H';
        *it = 'e';
        *it = 'l';
        *it = 'l';
        *it = 'o';

        EXPECT_EQ(str, "Hello");
    }

    TEST(iterator_test, back_inserter_with_std_format_to)
    {
        auto str = string{};
        std::format_to(back_inserter(str), "Batch {} ({})", 42, "Graphics");

        EXPECT_EQ(str, "Batch 42 (Graphics)");
    }

    TEST(iterator_test, front_inserter_with_deque)
    {
        auto deq = deque<int>{};
        auto it = front_inserter(deq);

        *it = 1;
        *it = 2;
        *it = 3;

        ASSERT_EQ(deq.size(), 3U);
        EXPECT_EQ(deq[0], 3);
        EXPECT_EQ(deq[1], 2);
        EXPECT_EQ(deq[2], 1);
    }

    TEST(iterator_test, inserter_with_vector)
    {
        auto vec = vector<int>(init_list, 1, 4);
        auto it = inserter(vec, vec.begin() + 1);

        *it = 2;
        *it = 3;

        ASSERT_EQ(vec.size(), 4U);
        EXPECT_EQ(vec[0], 1);
        EXPECT_EQ(vec[1], 2);
        EXPECT_EQ(vec[2], 3);
        EXPECT_EQ(vec[3], 4);
    }
} // namespace tempest
