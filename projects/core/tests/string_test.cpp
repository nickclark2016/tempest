#include <tempest/string.hpp>

#include <gtest/gtest.h>

TEST(string, default_constructor)
{
    tempest::string s;
    EXPECT_EQ(s.size(), 0);
    EXPECT_GE(s.capacity(), 0);
}

TEST(string, constructor_with_size)
{
    tempest::string s(10, 'a');
    EXPECT_EQ(s.size(), 10);
    EXPECT_GE(s.capacity(), 10);

    for (const auto& c : s)
    {
        EXPECT_EQ(c, 'a');
    }
}

TEST(string, constructor_with_size_greater_than_small_string)
{
    tempest::string s(100, 'a');
    EXPECT_EQ(s.size(), 100);
    EXPECT_GE(s.capacity(), 100);

    for (const auto& c : s)
    {
        EXPECT_EQ(c, 'a');
    }
}

TEST(string, copy_constructor)
{
    tempest::string s1(10, 'a');
    tempest::string s2(s1);
    EXPECT_EQ(s2.size(), 10);
    EXPECT_GE(s2.capacity(), 10);

    for (const auto& c : s2)
    {
        EXPECT_EQ(c, 'a');
    }

    // Ensure the old string is not affected
    for (const auto& c : s1)
    {
        EXPECT_EQ(c, 'a');
    }
}

TEST(string, move_constructor)
{
    tempest::string s1(10, 'a');
    tempest::string s2(std::move(s1));
    EXPECT_EQ(s2.size(), 10);
    EXPECT_GE(s2.capacity(), 10);

    for (const auto& c : s2)
    {
        EXPECT_EQ(c, 'a');
    }
}

TEST(string, copy_assignment)
{
    tempest::string s1(10, 'a');
    tempest::string s2;
    s2 = s1;
    EXPECT_EQ(s2.size(), 10);
    EXPECT_GE(s2.capacity(), 10);

    for (const auto& c : s2)
    {
        EXPECT_EQ(c, 'a');
    }

    // Ensure the old string is not affected
    for (const auto& c : s1)
    {
        EXPECT_EQ(c, 'a');
    }
}

TEST(string, copy_assignment_with_initial_contents)
{
    tempest::string s1(10, 'a');
    tempest::string s2(5, 'b');
    s2 = s1;
    EXPECT_EQ(s2.size(), 10);
    EXPECT_GE(s2.capacity(), 10);

    for (const auto& c : s2)
    {
        EXPECT_EQ(c, 'a');
    }

    // Ensure the old string is not affected
    for (const auto& c : s1)
    {
        EXPECT_EQ(c, 'a');
    }
}

TEST(string, copy_assignment_large_string_to_small_string)
{
    tempest::string s1(100, 'a');
    tempest::string s2(10, 'b');
    s2 = s1;
    EXPECT_EQ(s2.size(), 100);
    EXPECT_GE(s2.capacity(), 100);

    for (const auto& c : s2)
    {
        EXPECT_EQ(c, 'a');
    }

    // Ensure the old string is not affected
    for (const auto& c : s1)
    {
        EXPECT_EQ(c, 'a');
    }
}

TEST(string, copy_assignment_small_string_to_large_string)
{
    tempest::string s1(10, 'a');
    tempest::string s2(100, 'b');
    s2 = s1;
    EXPECT_EQ(s2.size(), 10);
    EXPECT_GE(s2.capacity(), 10);

    for (const auto& c : s2)
    {
        EXPECT_EQ(c, 'a');
    }

    // Ensure the old string is not affected
    for (const auto& c : s1)
    {
        EXPECT_EQ(c, 'a');
    }
}

TEST(string, move_assignment)
{
    tempest::string s1(10, 'a');
    tempest::string s2;
    s2 = std::move(s1);
    EXPECT_EQ(s2.size(), 10);
    EXPECT_GE(s2.capacity(), 10);

    for (const auto& c : s2)
    {
        EXPECT_EQ(c, 'a');
    }
}

TEST(string, move_assignment_with_initial_contents)
{
    tempest::string s1(10, 'a');
    tempest::string s2(5, 'b');
    s2 = std::move(s1);
    EXPECT_EQ(s2.size(), 10);
    EXPECT_GE(s2.capacity(), 10);

    for (const auto& c : s2)
    {
        EXPECT_EQ(c, 'a');
    }
}

TEST(string, move_assignment_large_string_to_small_string)
{
    tempest::string s1(100, 'a');
    tempest::string s2(10, 'b');
    s2 = std::move(s1);
    EXPECT_EQ(s2.size(), 100);
    EXPECT_GE(s2.capacity(), 100);

    for (const auto& c : s2)
    {
        EXPECT_EQ(c, 'a');
    }
}

TEST(string, move_assignment_small_string_to_large_string)
{
    tempest::string s1(10, 'a');
    tempest::string s2(100, 'b');
    s2 = std::move(s1);
    EXPECT_EQ(s2.size(), 10);
    EXPECT_GE(s2.capacity(), 10);

    for (const auto& c : s2)
    {
        EXPECT_EQ(c, 'a');
    }
}

TEST(string, insert_into_empty_string)
{
    tempest::string s;
    s.insert(s.begin(), 'a');
    EXPECT_EQ(s.size(), 1);
    EXPECT_GE(s.capacity(), 1);
    EXPECT_EQ(s[0], 'a');
}

TEST(string, insert_at_beginning_of_small_string_no_resize)
{
    tempest::string s(10, 'a');
    s.insert(s.begin(), 'b');
    EXPECT_EQ(s.size(), 11);
    EXPECT_GE(s.capacity(), 10);
    EXPECT_EQ(s[0], 'b');
    for (size_t i = 1; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_into_middle_of_small_string_no_resize)
{
    tempest::string s(10, 'a');
    s.insert(s.begin() + 5, 'b');
    EXPECT_EQ(s.size(), 11);
    EXPECT_GE(s.capacity(), 10);
    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    EXPECT_EQ(s[5], 'b');
    for (size_t i = 6; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_at_end_of_small_string_no_resize)
{
    tempest::string s(10, 'a');
    s.insert(s.end(), 'b');
    EXPECT_EQ(s.size(), 11);
    EXPECT_GE(s.capacity(), 10);
    for (size_t i = 0; i < s.size() - 1; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    EXPECT_EQ(s[10], 'b');
}

TEST(string, insert_at_beginning_of_large_string)
{
    tempest::string s(100, 'a');
    s.insert(s.begin(), 'b');
    EXPECT_EQ(s.size(), 101);
    EXPECT_GE(s.capacity(), 100);
    EXPECT_EQ(s[0], 'b');
    for (size_t i = 1; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_into_middle_of_large_string)
{
    tempest::string s(100, 'a');
    s.insert(s.begin() + 50, 'b');
    EXPECT_EQ(s.size(), 101);
    EXPECT_GE(s.capacity(), 100);
    for (size_t i = 0; i < 50; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    EXPECT_EQ(s[50], 'b');
    for (size_t i = 51; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_at_end_of_large_string)
{
    tempest::string s(100, 'a');
    s.insert(s.end(), 'b');
    EXPECT_EQ(s.size(), 101);
    EXPECT_GE(s.capacity(), 100);
    for (size_t i = 0; i < s.size() - 1; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    EXPECT_EQ(s[100], 'b');
}

TEST(string, insert_at_beginning_of_small_string_and_resize_to_large_string)
{
    tempest::string s(10, 'a');
    s.insert(s.begin(), 100, 'b');
    EXPECT_EQ(s.size(), 110);
    EXPECT_GE(s.capacity(), 110);
    for (size_t i = 0; i < 100; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
    for (size_t i = 100; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_into_middle_of_small_string_and_resize_to_large_string)
{
    tempest::string s(10, 'a');
    s.insert(s.begin() + 5, 100, 'b');
    EXPECT_EQ(s.size(), 110);
    EXPECT_GE(s.capacity(), 110);
    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    for (size_t i = 5; i < 105; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
    for (size_t i = 105; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_at_end_of_small_string_and_resize_to_large_string)
{
    tempest::string s(10, 'a');
    s.insert(s.end(), 100, 'b');
    EXPECT_EQ(s.size(), 110);
    EXPECT_GE(s.capacity(), 110);
    for (size_t i = 0; i < 10; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    for (size_t i = 10; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
}

TEST(string, insert_string_at_beginning_of_small_string_no_resize)
{
    tempest::string s(10, 'a');
    tempest::string t(5, 'b');
    s.insert(s.begin(), t);
    EXPECT_EQ(s.size(), 15);
    EXPECT_GE(s.capacity(), 10);
    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
    for (size_t i = 5; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_string_into_middle_of_small_string_no_resize)
{
    tempest::string s(10, 'a');
    tempest::string t(5, 'b');
    s.insert(s.begin() + 5, t);
    EXPECT_EQ(s.size(), 15);
    EXPECT_GE(s.capacity(), 10);
    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    for (size_t i = 5; i < 10; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
    for (size_t i = 10; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_string_at_end_of_small_string_no_resize)
{
    tempest::string s(10, 'a');
    tempest::string t(5, 'b');
    s.insert(s.end(), t);
    EXPECT_EQ(s.size(), 15);
    EXPECT_GE(s.capacity(), 10);
    for (size_t i = 0; i < 10; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    for (size_t i = 10; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
}

TEST(string, insert_string_at_beginning_of_large_string)
{
    tempest::string s(100, 'a');
    tempest::string t(5, 'b');
    s.insert(s.begin(), t);
    EXPECT_EQ(s.size(), 105);
    EXPECT_GE(s.capacity(), 100);
    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
    for (size_t i = 5; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_string_into_middle_of_large_string)
{
    tempest::string s(100, 'a');
    tempest::string t(5, 'b');
    s.insert(s.begin() + 50, t);
    EXPECT_EQ(s.size(), 105);
    EXPECT_GE(s.capacity(), 100);
    for (size_t i = 0; i < 50; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    for (size_t i = 50; i < 55; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
    for (size_t i = 55; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_string_at_end_of_large_string)
{
    tempest::string s(100, 'a');
    tempest::string t(5, 'b');
    s.insert(s.end(), t);
    EXPECT_EQ(s.size(), 105);
    EXPECT_GE(s.capacity(), 100);
    for (size_t i = 0; i < 100; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    for (size_t i = 100; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
}

TEST(string, insert_string_at_beginning_of_small_string_and_resize_to_large_string)
{
    tempest::string s(10, 'a');
    tempest::string t(100, 'b');
    s.insert(s.begin(), t);
    EXPECT_EQ(s.size(), 110);
    EXPECT_GE(s.capacity(), 110);
    for (size_t i = 0; i < 100; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
    for (size_t i = 100; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_string_into_middle_of_small_string_and_resize_to_large_string)
{
    tempest::string s(10, 'a');
    tempest::string t(100, 'b');
    s.insert(s.begin() + 5, t);
    EXPECT_EQ(s.size(), 110);
    EXPECT_GE(s.capacity(), 110);
    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    for (size_t i = 5; i < 105; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
    for (size_t i = 105; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_string_at_end_of_small_string_and_resize_to_large_string)
{
    tempest::string s(10, 'a');
    tempest::string t(100, 'b');
    s.insert(s.end(), t);
    EXPECT_EQ(s.size(), 110);
    EXPECT_GE(s.capacity(), 110);
    for (size_t i = 0; i < 10; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    for (size_t i = 10; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
}

TEST(string, insert_cstring_at_beginning_of_small_string_no_resize)
{
    tempest::string s(10, 'a');
    const char* t = "hello";
    s.insert(s.begin(), t);
    EXPECT_EQ(s.size(), 15);
    EXPECT_GE(s.capacity(), 10);

    EXPECT_EQ(s[0], 'h');
    EXPECT_EQ(s[1], 'e');
    EXPECT_EQ(s[2], 'l');
    EXPECT_EQ(s[3], 'l');
    EXPECT_EQ(s[4], 'o');

    for (size_t i = 5; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_cstring_into_middle_of_small_string_no_resize)
{
    tempest::string s(10, 'a');
    const char* t = "hello";
    s.insert(s.begin() + 5, t);
    EXPECT_EQ(s.size(), 15);
    EXPECT_GE(s.capacity(), 10);
    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    EXPECT_EQ(s[5], 'h');
    EXPECT_EQ(s[6], 'e');
    EXPECT_EQ(s[7], 'l');
    EXPECT_EQ(s[8], 'l');
    EXPECT_EQ(s[9], 'o');
    for (size_t i = 10; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_cstring_at_end_of_small_string_no_resize)
{
    tempest::string s(10, 'a');
    const char* t = "hello";
    s.insert(s.end(), t);
    EXPECT_EQ(s.size(), 15);
    EXPECT_GE(s.capacity(), 10);
    for (size_t i = 0; i < 10; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    EXPECT_EQ(s[10], 'h');
    EXPECT_EQ(s[11], 'e');
    EXPECT_EQ(s[12], 'l');
    EXPECT_EQ(s[13], 'l');
    EXPECT_EQ(s[14], 'o');
}

TEST(string, insert_cstring_at_beginning_of_large_string)
{
    tempest::string s(100, 'a');
    const char* t = "hello";
    s.insert(s.begin(), t);
    EXPECT_EQ(s.size(), 105);
    EXPECT_GE(s.capacity(), 100);
    EXPECT_EQ(s[0], 'h');
    EXPECT_EQ(s[1], 'e');
    EXPECT_EQ(s[2], 'l');
    EXPECT_EQ(s[3], 'l');
    EXPECT_EQ(s[4], 'o');
    for (size_t i = 5; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_cstring_into_middle_of_large_string)
{
    tempest::string s(100, 'a');
    const char* t = "hello";
    s.insert(s.begin() + 50, t);
    EXPECT_EQ(s.size(), 105);
    EXPECT_GE(s.capacity(), 100);
    for (size_t i = 0; i < 50; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    EXPECT_EQ(s[50], 'h');
    EXPECT_EQ(s[51], 'e');
    EXPECT_EQ(s[52], 'l');
    EXPECT_EQ(s[53], 'l');
    EXPECT_EQ(s[54], 'o');
    for (size_t i = 55; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_cstring_at_end_of_large_string)
{
    tempest::string s(100, 'a');
    const char* t = "hello";
    s.insert(s.end(), t);
    EXPECT_EQ(s.size(), 105);
    EXPECT_GE(s.capacity(), 100);
    for (size_t i = 0; i < 100; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    EXPECT_EQ(s[100], 'h');
    EXPECT_EQ(s[101], 'e');
    EXPECT_EQ(s[102], 'l');
    EXPECT_EQ(s[103], 'l');
    EXPECT_EQ(s[104], 'o');
}

TEST(string, insert_cstring_at_beginning_of_small_string_and_resize_to_large_string)
{
    tempest::string s(20, 'a');
    const char* t = "hello";
    s.insert(s.begin(), t);
    EXPECT_EQ(s.size(), 25);
    EXPECT_GE(s.capacity(), 25);
    EXPECT_EQ(s[0], 'h');
    EXPECT_EQ(s[1], 'e');
    EXPECT_EQ(s[2], 'l');
    EXPECT_EQ(s[3], 'l');
    EXPECT_EQ(s[4], 'o');
    for (size_t i = 5; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_cstring_into_middle_of_small_string_and_resize_to_large_string)
{
    tempest::string s(20, 'a');
    const char* t = "hello";
    s.insert(s.begin() + 10, t);
    EXPECT_EQ(s.size(), 25);
    EXPECT_GE(s.capacity(), 25);
    for (size_t i = 0; i < 10; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    EXPECT_EQ(s[10], 'h');
    EXPECT_EQ(s[11], 'e');
    EXPECT_EQ(s[12], 'l');
    EXPECT_EQ(s[13], 'l');
    EXPECT_EQ(s[14], 'o');
    for (size_t i = 15; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, insert_cstring_at_end_of_small_string_and_resize_to_large_string)
{
    tempest::string s(20, 'a');
    const char* t = "hello";
    s.insert(s.end(), t);
    EXPECT_EQ(s.size(), 25);
    EXPECT_GE(s.capacity(), 25);
    for (size_t i = 0; i < 20; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
    EXPECT_EQ(s[20], 'h');
    EXPECT_EQ(s[21], 'e');
    EXPECT_EQ(s[22], 'l');
    EXPECT_EQ(s[23], 'l');
    EXPECT_EQ(s[24], 'o');
}

TEST(string, erase_from_start_of_small_string)
{
    tempest::string s(10, 'a');
    s.erase(s.begin());
    EXPECT_EQ(s.size(), 9);
    EXPECT_GE(s.capacity(), 10);
    for (size_t i = 0; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, erase_from_middle_of_small_string)
{
    tempest::string s(5, 'a');
    s.append(5, 'b');

    s.erase(s.begin() + 5);
    EXPECT_EQ(s.size(), 9);
    EXPECT_GE(s.capacity(), 10);
    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 5; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
}

TEST(string, erase_from_end_of_small_string)
{
    tempest::string s(10, 'a');
    s.erase(s.end() - 1);
    EXPECT_EQ(s.size(), 9);
    EXPECT_GE(s.capacity(), 10);
    for (size_t i = 0; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, erase_from_start_of_large_string)
{
    tempest::string s(100, 'a');
    s.erase(s.begin());
    EXPECT_EQ(s.size(), 99);
    EXPECT_GE(s.capacity(), 100);
    for (size_t i = 0; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, erase_from_middle_of_large_string)
{
    tempest::string s(50, 'a');
    s.append(50, 'b');

    s.erase(s.begin() + 50);
    EXPECT_EQ(s.size(), 99);
    EXPECT_GE(s.capacity(), 100);
    for (size_t i = 0; i < 50; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 50; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
}

TEST(string, erase_from_end_of_large_string)
{
    tempest::string s(100, 'a');
    s.erase(s.end() - 1);
    EXPECT_EQ(s.size(), 99);
    EXPECT_GE(s.capacity(), 100);
    for (size_t i = 0; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, erase_range_from_start_of_small_string)
{
    tempest::string s(10, 'a');
    s.erase(s.begin(), s.begin() + 5);
    EXPECT_EQ(s.size(), 5);
    EXPECT_GE(s.capacity(), 10);
    for (size_t i = 0; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, erase_range_from_middle_of_small_string)
{
    tempest::string s(5, 'a');
    s.append(5, 'b');

    s.erase(s.begin() + 2, s.begin() + 7);
    EXPECT_EQ(s.size(), 5);
    EXPECT_GE(s.capacity(), 10);
    EXPECT_EQ(s[0], 'a');
    EXPECT_EQ(s[1], 'a');
    EXPECT_EQ(s[2], 'b');
    EXPECT_EQ(s[3], 'b');
    EXPECT_EQ(s[4], 'b');
}

TEST(string, erase_range_from_end_of_small_string)
{
    tempest::string s(10, 'a');
    s.erase(s.begin() + 5, s.end());
    EXPECT_EQ(s.size(), 5);
    EXPECT_GE(s.capacity(), 10);
    for (size_t i = 0; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, erase_range_from_start_of_large_string)
{
    tempest::string s(100, 'a');
    s.erase(s.begin(), s.begin() + 50);
    EXPECT_EQ(s.size(), 50);
    EXPECT_GE(s.capacity(), 100);
    for (size_t i = 0; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, erase_range_from_middle_of_large_string)
{
    tempest::string s(50, 'a');
    s.append(50, 'b');

    s.erase(s.begin() + 20, s.begin() + 80);
    EXPECT_EQ(s.size(), 40);
    EXPECT_GE(s.capacity(), 100);
    for (size_t i = 0; i < 20; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 20; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
}

TEST(string, erase_range_from_end_of_large_string)
{
    tempest::string s(100, 'a');
    s.erase(s.begin() + 50, s.end());
    EXPECT_EQ(s.size(), 50);
    EXPECT_GE(s.capacity(), 100);
    for (size_t i = 0; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, erase_all)
{
    tempest::string s(10, 'a');
    s.erase(s.begin(), s.end());
    EXPECT_EQ(s.size(), 0);
    EXPECT_GE(s.capacity(), 10);
}

TEST(string, clear_small_string)
{
    tempest::string s(10, 'a');
    s.clear();
    EXPECT_EQ(s.size(), 0);
    EXPECT_GE(s.capacity(), 10);
}

TEST(string, clear_large_string)
{
    tempest::string s(100, 'a');
    s.clear();
    EXPECT_EQ(s.size(), 0);
    EXPECT_GE(s.capacity(), 100);
}

TEST(string, replace_small_string_start_no_resize)
{
    tempest::string s(10, 'a');
    s.replace(s.begin(), s.begin() + 5, 5, 'b');
    EXPECT_EQ(s.size(), 10);
    EXPECT_GE(s.capacity(), 10);

    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 5; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_small_string_start_with_shrink)
{
    tempest::string s(10, 'a');
    s.replace(s.begin(), s.begin() + 5, 2, 'b');
    EXPECT_EQ(s.size(), 7);
    EXPECT_GE(s.capacity(), 10);

    for (size_t i = 0; i < 2; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 2; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_small_string_start_with_growth)
{
    tempest::string s(10, 'a');
    s.replace(s.begin(), s.begin() + 5, 10, 'b');
    EXPECT_EQ(s.size(), 15);
    EXPECT_GE(s.capacity(), 15);

    for (size_t i = 0; i < 10; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 10; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_small_string_middle_no_resize)
{
    tempest::string s(10, 'a');
    s.replace(s.begin() + 2, s.begin() + 7, 5, 'b');
    EXPECT_EQ(s.size(), 10);
    EXPECT_GE(s.capacity(), 10);

    for (size_t i = 0; i < 2; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 2; i < 7; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 7; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_small_string_middle_with_shrink)
{
    tempest::string s(10, 'a');
    s.replace(s.begin() + 2, s.begin() + 7, 2, 'b');
    EXPECT_EQ(s.size(), 7);
    EXPECT_GE(s.capacity(), 10);

    for (size_t i = 0; i < 2; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 2; i < 4; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 4; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_small_string_middle_with_growth)
{
    tempest::string s(10, 'a');
    s.replace(s.begin() + 2, s.begin() + 7, 10, 'b');
    EXPECT_EQ(s.size(), 15);
    EXPECT_GE(s.capacity(), 15);

    for (size_t i = 0; i < 2; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 2; i < 12; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 12; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_small_string_end_no_resize)
{
    tempest::string s(10, 'a');
    s.replace(s.end() - 5, s.end(), 5, 'b');
    EXPECT_EQ(s.size(), 10);
    EXPECT_GE(s.capacity(), 10);

    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 5; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
}

TEST(string, replace_small_string_end_with_shrink)
{
    tempest::string s(10, 'a');
    s.replace(s.end() - 5, s.end(), 2, 'b');
    EXPECT_EQ(s.size(), 7);
    EXPECT_GE(s.capacity(), 10);

    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 5; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
}

TEST(string, replace_small_string_end_with_growth)
{
    tempest::string s(10, 'a');
    s.replace(s.end() - 5, s.end(), 10, 'b');
    EXPECT_EQ(s.size(), 15);
    EXPECT_GE(s.capacity(), 15);

    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 5; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
}

TEST(string, replace_large_string_start_no_resize)
{
    tempest::string s(100, 'a');
    s.replace(s.begin(), s.begin() + 50, 50, 'b');
    EXPECT_EQ(s.size(), 100);
    EXPECT_GE(s.capacity(), 100);

    for (size_t i = 0; i < 50; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 50; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_large_string_start_with_shrink)
{
    tempest::string s(100, 'a');
    s.replace(s.begin(), s.begin() + 50, 20, 'b');
    EXPECT_EQ(s.size(), 70);
    EXPECT_GE(s.capacity(), 100);

    for (size_t i = 0; i < 20; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 20; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_large_string_start_with_growth)
{
    tempest::string s(100, 'a');
    s.replace(s.begin(), s.begin() + 50, 100, 'b');
    EXPECT_EQ(s.size(), 150);
    EXPECT_GE(s.capacity(), 150);

    for (size_t i = 0; i < 100; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 100; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_large_string_middle_no_resize)
{
    tempest::string s(100, 'a');
    s.replace(s.begin() + 25, s.begin() + 75, 50, 'b');
    EXPECT_EQ(s.size(), 100);
    EXPECT_GE(s.capacity(), 100);

    for (size_t i = 0; i < 25; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 25; i < 75; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 75; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_large_string_middle_with_shrink)
{
    tempest::string s(100, 'a');
    s.replace(s.begin() + 25, s.begin() + 75, 20, 'b');
    EXPECT_EQ(s.size(), 70);
    EXPECT_GE(s.capacity(), 100);

    for (size_t i = 0; i < 25; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 25; i < 45; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 45; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_large_string_middle_with_growth)
{
    tempest::string s(100, 'a');
    s.replace(s.begin() + 25, s.begin() + 75, 100, 'b');
    EXPECT_EQ(s.size(), 150);
    EXPECT_GE(s.capacity(), 150);

    for (size_t i = 0; i < 25; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 25; i < 125; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 125; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_start_of_small_string_with_large_string)
{
    tempest::string s(10, 'a');
    tempest::string t(100, 'b');
    s.replace(s.begin(), s.begin() + 5, t);
    EXPECT_EQ(s.size(), 105);
    EXPECT_GE(s.capacity(), 105);

    for (size_t i = 0; i < 100; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 100; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_middle_of_small_string_with_large_string)
{
    tempest::string s(10, 'a');
    tempest::string t(100, 'b');
    s.replace(s.begin() + 2, s.begin() + 7, t);
    EXPECT_EQ(s.size(), 105);
    EXPECT_GE(s.capacity(), 105);

    for (size_t i = 0; i < 2; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 2; i < 102; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 102; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_end_of_small_string_with_large_string)
{
    tempest::string s(10, 'a');
    tempest::string t(100, 'b');
    s.replace(s.end() - 5, s.end(), t);
    EXPECT_EQ(s.size(), 105);
    EXPECT_GE(s.capacity(), 105);

    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 5; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
}

TEST(string, replace_start_of_large_string_with_small_string)
{
    tempest::string s(100, 'a');
    tempest::string t(10, 'b');
    s.replace(s.begin(), s.begin() + 50, t);
    EXPECT_EQ(s.size(), 60);
    EXPECT_GE(s.capacity(), 100);

    for (size_t i = 0; i < 10; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 10; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_middle_of_large_string_with_small_string)
{
    tempest::string s(100, 'a');
    tempest::string t(10, 'b');
    s.replace(s.begin() + 25, s.begin() + 75, t);
    EXPECT_EQ(s.size(), 60);
    EXPECT_GE(s.capacity(), 100);

    for (size_t i = 0; i < 25; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 25; i < 35; ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }

    for (size_t i = 35; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_end_of_large_string_with_small_string)
{
    tempest::string s(100, 'a');
    tempest::string t(10, 'b');
    s.replace(s.end() - 5, s.end(), t);
    EXPECT_EQ(s.size(), 105);
    EXPECT_GE(s.capacity(), 105);

    for (size_t i = 0; i < 95; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    for (size_t i = 95; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'b');
    }
}

TEST(string, replace_start_of_small_string_with_cstring)
{
    tempest::string s(10, 'a');
    const char* t = "hello";
    s.replace(s.begin(), s.begin() + 5, t);
    EXPECT_EQ(s.size(), 10);
    EXPECT_GE(s.capacity(), 10);

    EXPECT_EQ(s[0], 'h');
    EXPECT_EQ(s[1], 'e');
    EXPECT_EQ(s[2], 'l');
    EXPECT_EQ(s[3], 'l');
    EXPECT_EQ(s[4], 'o');

    for (size_t i = 5; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_middle_of_small_string_with_cstring)
{
    tempest::string s(10, 'a');
    const char* t = "hello";
    s.replace(s.begin() + 2, s.begin() + 7, t);
    EXPECT_EQ(s.size(), 10);
    EXPECT_GE(s.capacity(), 10);

    for (size_t i = 0; i < 2; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    EXPECT_EQ(s[2], 'h');
    EXPECT_EQ(s[3], 'e');
    EXPECT_EQ(s[4], 'l');
    EXPECT_EQ(s[5], 'l');
    EXPECT_EQ(s[6], 'o');

    for (size_t i = 7; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_end_of_small_string_with_cstring)
{
    tempest::string s(10, 'a');
    const char* t = "hello";
    s.replace(s.end() - 5, s.end(), t);
    EXPECT_EQ(s.size(), 10);
    EXPECT_GE(s.capacity(), 10);

    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    EXPECT_EQ(s[5], 'h');
    EXPECT_EQ(s[6], 'e');
    EXPECT_EQ(s[7], 'l');
    EXPECT_EQ(s[8], 'l');
    EXPECT_EQ(s[9], 'o');
}

TEST(string, replace_start_of_large_string_with_cstring)
{
    tempest::string s(100, 'a');
    const char* t = "hello";
    s.replace(s.begin(), s.begin() + 50, t);
    EXPECT_EQ(s.size(), 55);
    EXPECT_GE(s.capacity(), 55);

    EXPECT_EQ(s[0], 'h');
    EXPECT_EQ(s[1], 'e');
    EXPECT_EQ(s[2], 'l');
    EXPECT_EQ(s[3], 'l');
    EXPECT_EQ(s[4], 'o');

    for (size_t i = 5; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_middle_of_large_string_with_cstring)
{
    tempest::string s(100, 'a');
    const char* t = "hello";
    s.replace(s.begin() + 25, s.begin() + 75, t);
    EXPECT_EQ(s.size(), 55);
    EXPECT_GE(s.capacity(), 55);

    for (size_t i = 0; i < 25; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    EXPECT_EQ(s[25], 'h');
    EXPECT_EQ(s[26], 'e');
    EXPECT_EQ(s[27], 'l');
    EXPECT_EQ(s[28], 'l');
    EXPECT_EQ(s[29], 'o');

    for (size_t i = 30; i < s.size(); ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(string, replace_end_of_large_string_with_cstring)
{
    tempest::string s(100, 'a');
    const char* t = "hello";
    s.replace(s.end() - 5, s.end(), t);
    EXPECT_EQ(s.size(), 100);
    EXPECT_GE(s.capacity(), 100);

    for (size_t i = 0; i < 95; ++i)
    {
        EXPECT_EQ(s[i], 'a');
    }

    EXPECT_EQ(s[95], 'h');
    EXPECT_EQ(s[96], 'e');
    EXPECT_EQ(s[97], 'l');
    EXPECT_EQ(s[98], 'l');
    EXPECT_EQ(s[99], 'o');
}

TEST(string, search)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::search(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, search_contains_multiple_instances)
{
    tempest::string s = "hello world hello";
    tempest::string t = "hello";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::search(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.begin());

    it = tempest::search(it + 1, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.begin() + 12);
}

TEST(string, search_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "worlds";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::search(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_against_char)
{
    tempest::string s = "hello world";
    char t = 'w';

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search(s_begin, s_end, t);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, search_against_char_not_found)
{
    tempest::string s = "hello world";
    char t = 'z';

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search(s_begin, s_end, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_string)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    auto it = tempest::search(s, t);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, search_string_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "worlds";

    auto it = tempest::search(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_string_vs_cstring)
{
    tempest::string s = "hello world";
    const char* t = "world";

    auto it = tempest::search(s, t);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, search_string_vs_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "worlds";

    auto it = tempest::search(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_string_vs_char)
{
    tempest::string s = "hello world";
    char t = 'w';

    auto it = tempest::search(s, t);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, search_string_vs_char_not_found)
{
    tempest::string s = "hello world";
    char t = 'z';

    auto it = tempest::search(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_first_of)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::search_first_of(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.begin() + 2);
}

TEST(string, search_first_of_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "z";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::search_first_of(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_first_of_with_cstring)
{
    tempest::string s = "hello world";
    const char* t = "world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_first_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.begin() + 2);
}

TEST(string, search_first_of_with_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "z";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_first_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_first_of_with_char)
{
    tempest::string s = "hello world";
    char t = 'w';

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_first_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, search_first_of_with_char_not_found)
{
    tempest::string s = "hello world";
    char t = 'z';

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_first_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_first_of_string)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    auto it = tempest::search_first_of(s, t);

    EXPECT_EQ(it, s.begin() + 2);
}

TEST(string, search_first_of_string_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "z";

    auto it = tempest::search_first_of(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_first_of_string_vs_cstring)
{
    tempest::string s = "hello world";
    const char* t = "world";

    auto it = tempest::search_first_of(s, t);

    EXPECT_EQ(it, s.begin() + 2);
}

TEST(string, search_first_of_string_vs_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "z";

    auto it = tempest::search_first_of(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_first_of_string_vs_char)
{
    tempest::string s = "hello world";
    char t = 'w';

    auto it = tempest::search_first_of(s, t);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, search_first_of_string_vs_char_not_found)
{
    tempest::string s = "hello world";
    char t = 'z';

    auto it = tempest::search_first_of(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, reverse_search)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::reverse_search(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, reverse_search_multiple_instances)
{
    tempest::string s = "hello world hello";
    tempest::string t = "hello";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::reverse_search(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.begin() + 12);

    it = tempest::reverse_search(s_begin, it, t_begin, t_end);

    EXPECT_EQ(it, s.begin());
}

TEST(string, reverse_search_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "worlds";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::reverse_search(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.end());
}

TEST(string, reverse_search_cstring)
{
    tempest::string s = "hello world";
    const char* t = "world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::reverse_search(s_begin, s_end, t);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, reverse_search_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "worlds";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::reverse_search(s_begin, s_end, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, reverse_search_char)
{
    tempest::string s = "hello world";
    char t = 'w';

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::reverse_search(s_begin, s_end, t);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, reverse_search_char_not_found)
{
    tempest::string s = "hello world";
    char t = 'z';

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::reverse_search(s_begin, s_end, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, reverse_search_string)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    auto it = tempest::reverse_search(s, t);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, reverse_search_string_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "worlds";

    auto it = tempest::reverse_search(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, reverse_search_string_vs_cstring)
{
    tempest::string s = "hello world";
    const char* t = "world";

    auto it = tempest::reverse_search(s, t);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, reverse_search_string_vs_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "worlds";

    auto it = tempest::reverse_search(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, reverse_search_string_vs_char)
{
    tempest::string s = "hello world";
    char t = 'w';

    auto it = tempest::reverse_search(s, t);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, reverse_search_string_vs_char_not_found)
{
    tempest::string s = "hello world";
    char t = 'z';

    auto it = tempest::reverse_search(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_last_of)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::search_last_of(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.begin() + 10);
}

TEST(string, search_last_of_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "z";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::search_last_of(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_last_of_with_cstring)
{
    tempest::string s = "hello world";
    const char* t = "world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_last_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.begin() + 10);
}

TEST(string, search_last_of_with_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "z";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_last_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_last_of_with_char)
{
    tempest::string s = "hello world";
    char t = 'w';

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_last_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, search_last_of_with_char_not_found)
{
    tempest::string s = "hello world";
    char t = 'z';

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_last_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_last_of_string)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    auto it = tempest::search_last_of(s, t);

    EXPECT_EQ(it, s.begin() + 10);
}

TEST(string, search_last_of_string_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "z";

    auto it = tempest::search_last_of(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_last_of_string_vs_cstring)
{
    tempest::string s = "hello world";
    const char* t = "world";

    auto it = tempest::search_last_of(s, t);

    EXPECT_EQ(it, s.begin() + 10);
}

TEST(string, search_last_of_string_vs_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "z";

    auto it = tempest::search_last_of(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_last_of_string_vs_char)
{
    tempest::string s = "hello world";
    char t = 'w';

    auto it = tempest::search_last_of(s, t);

    EXPECT_EQ(it, s.begin() + 6);
}

TEST(string, search_last_of_string_vs_char_not_found)
{
    tempest::string s = "hello world";
    char t = 'z';

    auto it = tempest::search_last_of(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_first_not_of)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::search_first_not_of(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.begin());
}

TEST(string, search_first_not_of_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "hello world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::search_first_not_of(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_first_not_of_middle_of_string)
{
    tempest::string s = "hello world";
    tempest::string t = "hello";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::search_first_not_of(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.begin() + 5);
}

TEST(string, search_first_not_of_with_cstring)
{
    tempest::string s = "hello world";
    const char* t = "world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_first_not_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.begin());
}

TEST(string, search_first_not_of_with_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "hello world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_first_not_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_first_not_of_with_char)
{
    tempest::string s = "hello world";
    char t = 'w';

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_first_not_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.begin());
}

TEST(string, search_first_not_of_with_char_at_start)
{
    tempest::string s = "hello world";
    char t = 'h';

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_first_not_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.begin() + 1);
}

TEST(string, search_first_not_of_string)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    auto it = tempest::search_first_not_of(s, t);

    EXPECT_EQ(it, s.begin());
}

TEST(string, search_first_not_of_string_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "hello world";

    auto it = tempest::search_first_not_of(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_first_not_of_string_vs_cstring)
{
    tempest::string s = "hello world";
    const char* t = "world";

    auto it = tempest::search_first_not_of(s, t);

    EXPECT_EQ(it, s.begin());
}

TEST(string, search_first_not_of_string_vs_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "hello world";

    auto it = tempest::search_first_not_of(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_first_not_of_string_vs_char)
{
    tempest::string s = "hello world";
    char t = 'w';

    auto it = tempest::search_first_not_of(s, t);

    EXPECT_EQ(it, s.begin());
}

TEST(string, search_first_not_of_string_vs_char_at_start)
{
    tempest::string s = "hello world";
    char t = 'h';

    auto it = tempest::search_first_not_of(s, t);

    EXPECT_EQ(it, s.begin() + 1);
}

TEST(string, search_last_not_of)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::search_last_not_of(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.begin() + 5);
}

TEST(string, search_last_not_of_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "hello world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::search_last_not_of(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_last_not_of_middle_of_string)
{
    tempest::string s = "hello world";
    tempest::string t = "hello";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();
    auto t_begin = t.cbegin();
    auto t_end = t.cend();

    auto it = tempest::search_last_not_of(s_begin, s_end, t_begin, t_end);

    EXPECT_EQ(it, s.begin() + 10);
}

TEST(string, search_last_not_of_with_cstring)
{
    tempest::string s = "hello world";
    const char* t = "world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_last_not_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.begin() + 5);
}

TEST(string, search_last_not_of_with_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "hello world";

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_last_not_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_last_not_of_with_char)
{
    tempest::string s = "hello world";
    char t = 'w';

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_last_not_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.begin() + 10);
}

TEST(string, search_last_not_of_with_char_at_end)
{
    tempest::string s = "hello world";
    char t = 'd';

    auto s_begin = s.cbegin();
    auto s_end = s.cend();

    auto it = tempest::search_last_not_of(s_begin, s_end, t);

    EXPECT_EQ(it, s.begin() + 9);
}

TEST(string, search_last_not_of_string)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    auto it = tempest::search_last_not_of(s, t);

    EXPECT_EQ(it, s.begin() + 5);
}

TEST(string, search_last_not_of_string_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "hello world";

    auto it = tempest::search_last_not_of(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_last_not_of_string_vs_cstring)
{
    tempest::string s = "hello world";
    const char* t = "world";

    auto it = tempest::search_last_not_of(s, t);

    EXPECT_EQ(it, s.begin() + 5);
}

TEST(string, search_last_not_of_string_vs_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "hello world";

    auto it = tempest::search_last_not_of(s, t);

    EXPECT_EQ(it, s.end());
}

TEST(string, search_last_not_of_string_vs_char)
{
    tempest::string s = "hello world";
    char t = 'w';

    auto it = tempest::search_last_not_of(s, t);

    EXPECT_EQ(it, s.begin() + 10);
}

TEST(string, search_last_not_of_string_vs_char_at_end)
{
    tempest::string s = "hello world";
    char t = 'd';

    auto it = tempest::search_last_not_of(s, t);

    EXPECT_EQ(it, s.begin() + 9);
}

TEST(string, equality)
{
    tempest::string s = "hello world";
    tempest::string t = "hello world";

    EXPECT_EQ(s, t);
}

TEST(string, inequality)
{
    tempest::string s = "hello world";
    tempest::string t = "hello world!";

    EXPECT_NE(s, t);
}

TEST(string, less_than)
{
    tempest::string s = "hello world";
    tempest::string t = "hello world!";

    EXPECT_LT(s, t);
}

TEST(string, less_than_or_equal_equal)
{
    tempest::string s = "hello world";
    tempest::string t = "hello world";

    EXPECT_LE(s, t);
}

TEST(string, less_that_or_equal_less_than)
{
    tempest::string s = "hello world";
    tempest::string t = "hello world!";

    EXPECT_LE(s, t);
}

TEST(string, greater_than)
{
    tempest::string s = "hello world!";
    tempest::string t = "hello world";

    EXPECT_GT(s, t);
}

TEST(string, greater_than_or_equal_equal)
{
    tempest::string s = "hello world";
    tempest::string t = "hello world";

    EXPECT_GE(s, t);
}

TEST(string, greater_than_or_equal_greater_than)
{
    tempest::string s = "hello world!";
    tempest::string t = "hello world";

    EXPECT_GE(s, t);
}

TEST(string, starts_with)
{
    tempest::string s = "hello world";
    tempest::string t = "hello";

    EXPECT_TRUE(tempest::starts_with(s.begin(), s.end(), t.begin(), t.end()));
}

TEST(string, starts_with_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    EXPECT_FALSE(tempest::starts_with(s.begin(), s.end(), t.begin(), t.end()));
}

TEST(string, starts_with_cstring)
{
    tempest::string s = "hello world";
    const char* t = "hello";

    EXPECT_TRUE(tempest::starts_with(s.begin(), s.end(), t));
}

TEST(string, starts_with_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "world";

    EXPECT_FALSE(tempest::starts_with(s.begin(), s.end(), t));
}

TEST(string, starts_with_char)
{
    tempest::string s = "hello world";
    char t = 'h';

    EXPECT_TRUE(tempest::starts_with(s.begin(), s.end(), t));
}

TEST(string, starts_with_char_not_found)
{
    tempest::string s = "hello world";
    char t = 'w';

    EXPECT_FALSE(tempest::starts_with(s.begin(), s.end(), t));
}

TEST(string, starts_with_string)
{
    tempest::string s = "hello world";
    tempest::string t = "hello";

    EXPECT_TRUE(tempest::starts_with(s, t));
}

TEST(string, starts_with_string_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    EXPECT_FALSE(tempest::starts_with(s, t));
}

TEST(string, starts_with_string_vs_cstring)
{
    tempest::string s = "hello world";
    const char* t = "hello";

    EXPECT_TRUE(tempest::starts_with(s, t));
}

TEST(string, starts_with_string_vs_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "world";

    EXPECT_FALSE(tempest::starts_with(s, t));
}

TEST(string, starts_with_string_vs_char)
{
    tempest::string s = "hello world";
    char t = 'h';

    EXPECT_TRUE(tempest::starts_with(s, t));
}

TEST(string, starts_with_string_vs_char_not_found)
{
    tempest::string s = "hello world";
    char t = 'w';

    EXPECT_FALSE(tempest::starts_with(s, t));
}

TEST(string, ends_with)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    EXPECT_TRUE(tempest::ends_with(s.begin(), s.end(), t.begin(), t.end()));
}

TEST(string, ends_with_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "hello";

    EXPECT_FALSE(tempest::ends_with(s.begin(), s.end(), t.begin(), t.end()));
}

TEST(string, ends_with_cstring)
{
    tempest::string s = "hello world";
    const char* t = "world";

    EXPECT_TRUE(tempest::ends_with(s.begin(), s.end(), t));
}

TEST(string, ends_with_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "hello";

    EXPECT_FALSE(tempest::ends_with(s.begin(), s.end(), t));
}

TEST(string, ends_with_char)
{
    tempest::string s = "hello world";
    char t = 'd';

    EXPECT_TRUE(tempest::ends_with(s.begin(), s.end(), t));
}

TEST(string, ends_with_char_not_found)
{
    tempest::string s = "hello world";
    char t = 'h';

    EXPECT_FALSE(tempest::ends_with(s.begin(), s.end(), t));
}

TEST(string, ends_with_string)
{
    tempest::string s = "hello world";
    tempest::string t = "world";

    EXPECT_TRUE(tempest::ends_with(s, t));
}

TEST(string, ends_with_string_not_found)
{
    tempest::string s = "hello world";
    tempest::string t = "hello";

    EXPECT_FALSE(tempest::ends_with(s, t));
}

TEST(string, ends_with_string_vs_cstring)
{
    tempest::string s = "hello world";
    const char* t = "world";

    EXPECT_TRUE(tempest::ends_with(s, t));
}

TEST(string, ends_with_string_vs_cstring_not_found)
{
    tempest::string s = "hello world";
    const char* t = "hello";

    EXPECT_FALSE(tempest::ends_with(s, t));
}

TEST(string, ends_with_string_vs_char)
{
    tempest::string s = "hello world";
    char t = 'd';

    EXPECT_TRUE(tempest::ends_with(s, t));
}

TEST(string, ends_with_string_vs_char_not_found)
{
    tempest::string s = "hello world";
    char t = 'h';

    EXPECT_FALSE(tempest::ends_with(s, t));
}
