#include <gtest/gtest.h>

#include <tempest/span.hpp>
#include <tempest/vector.hpp>

#include <numeric>

TEST(span, default_construct_dynamic)
{
    tempest::core::span<int> s;
    EXPECT_EQ(s.size(), 0);
    EXPECT_EQ(s.data(), nullptr);
}

TEST(span, construct_from_vector)
{
    tempest::core::vector<int> v(10, 42);
    tempest::core::span<int> s(v);
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), v.data());

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, construct_from_pointer)
{
    tempest::core::vector<int> v(10, 42);
    tempest::core::span<int> s(v.data(), v.size());
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), v.data());

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, construct_from_array)
{
    int arr[10];
    std::fill(std::begin(arr), std::end(arr), 42);

    tempest::core::span<int> s(arr);
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), arr);

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, construct_from_const_array)
{
    const int arr[10] = {42, 42, 42, 42, 42, 42, 42, 42, 42, 42};
    
    tempest::core::span<const int> s(arr);
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), arr);

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, construct_from_std_array)
{
    std::array<int, 10> arr;
    std::fill(arr.begin(), arr.end(), 42);

    tempest::core::span<int> s(arr);
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), arr.data());

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, construct_from_const_std_array)
{
    const std::array<int, 10> arr = {42, 42, 42, 42, 42, 42, 42, 42, 42, 42};

    tempest::core::span<const int> s(arr);
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), arr.data());

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, static_length_from_array)
{
    int arr[10];
    std::fill(std::begin(arr), std::end(arr), 42);

    tempest::core::span<int, 10> s(arr);
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), arr);

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, static_length_from_const_array)
{
    const int arr[10] = {42, 42, 42, 42, 42, 42, 42, 42, 42, 42};

    tempest::core::span<const int, 10> s(arr);
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), arr);

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, static_length_from_std_array)
{
    std::array<int, 10> arr;
    std::fill(arr.begin(), arr.end(), 42);

    tempest::core::span<int, 10> s(arr);
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), arr.data());

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, static_length_from_const_std_array)
{
    const std::array<int, 10> arr = {42, 42, 42, 42, 42, 42, 42, 42, 42, 42};

    tempest::core::span<const int, 10> s(arr);
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), arr.data());

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, template_deduction_from_array)
{
    int arr[10];
    std::fill(std::begin(arr), std::end(arr), 42);

    tempest::core::span s(arr);
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), arr);

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, template_deduction_from_const_array)
{
    const int arr[10] = {42, 42, 42, 42, 42, 42, 42, 42, 42, 42};

    tempest::core::span s(arr);
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), arr);

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, template_deduction_from_std_array)
{
    std::array<int, 10> arr;
    std::fill(arr.begin(), arr.end(), 42);

    tempest::core::span s(arr);
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), arr.data());

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, template_deduction_from_const_std_array)
{
    const std::array<int, 10> arr = {42, 42, 42, 42, 42, 42, 42, 42, 42, 42};

    tempest::core::span s(arr);
    EXPECT_EQ(s.size(), 10);
    EXPECT_EQ(s.data(), arr.data());

    for (const auto& i : s)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(span, front_and_back)
{
    tempest::core::vector<int> v(10, 42);
    tempest::core::span<int> s(v);
    EXPECT_EQ(s.front(), 42);
    EXPECT_EQ(s.back(), 42);
}

TEST(span, subspan)
{
    // fill vector with increasing values
    tempest::core::vector<int> v(10);
    std::iota(v.begin(), v.end(), 0);

    // create a span from the vector
    tempest::core::span<int> s(v);

    // create a subspan from the span
    tempest::core::span<int> sub = s.subspan(2, 5);

    // check the size of the subspan
    EXPECT_EQ(sub.size(), 5);

    // check the values of the subspan
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(sub[i], i + 2);
    }
}

TEST(span, subspan_static)
{
    // fill vector with increasing values
    tempest::core::vector<int> v(10);
    std::iota(v.begin(), v.end(), 0);

    // create a span from the vector
    tempest::core::span<int> s(v);

    // create a subspan from the span
    tempest::core::span<int, 5> sub = s.subspan<2, 5>();

    // check the size of the subspan
    EXPECT_EQ(sub.size(), 5);

    // check the values of the subspan
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(sub[i], i + 2);
    }
}

TEST(span, subspan_static_dynamic)
{
    // fill vector with increasing values
    tempest::core::vector<int> v(10);
    std::iota(v.begin(), v.end(), 0);

    // create a span from the vector
    tempest::core::span<int> s(v);

    // create a subspan from the span
    tempest::core::span<int, 8> sub = s.subspan<2>();

    // check the size of the subspan
    EXPECT_EQ(sub.size(), 8);

    // check the values of the subspan
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(sub[i], i + 2);
    }
}

TEST(span, subspan_dynamic_static)
{
    // fill vector with increasing values
    tempest::core::vector<int> v(10);
    std::iota(v.begin(), v.end(), 0);

    // create a span from the vector
    tempest::core::span<int> s(v);

    // create a subspan from the span
    tempest::core::span<int, 8> sub = s.subspan(2);

    // check the size of the subspan
    EXPECT_EQ(sub.size(), 8);

    // check the values of the subspan
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(sub[i], i + 2);
    }
}

TEST(span, first_dynamic)
{
    // fill vector with increasing values
    tempest::core::vector<int> v(10);
    std::iota(v.begin(), v.end(), 0);

    // create a span from the vector
    tempest::core::span<int> s(v);

    // create a subspan from the span
    tempest::core::span<int> sub = s.first(5);

    // check the size of the subspan
    EXPECT_EQ(sub.size(), 5);

    // check the values of the subspan
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(sub[i], i);
    }
}

TEST(span, first_static)
{
    // fill vector with increasing values
    tempest::core::vector<int> v(10);
    std::iota(v.begin(), v.end(), 0);

    // create a span from the vector
    tempest::core::span<int> s(v);

    // create a subspan from the span
    tempest::core::span<int, 5> sub = s.first<5>();

    // check the size of the subspan
    EXPECT_EQ(sub.size(), 5);

    // check the values of the subspan
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(sub[i], i);
    }
}

TEST(span, last_dynamic)
{
    // fill vector with increasing values
    tempest::core::vector<int> v(10);
    std::iota(v.begin(), v.end(), 0);

    // create a span from the vector
    tempest::core::span<int> s(v);

    // create a subspan from the span
    tempest::core::span<int> sub = s.last(5);

    // check the size of the subspan
    EXPECT_EQ(sub.size(), 5);

    // check the values of the subspan
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(sub[i], i + 5);
    }
}

TEST(span, last_static)
{
    // fill vector with increasing values
    tempest::core::vector<int> v(10);
    std::iota(v.begin(), v.end(), 0);

    // create a span from the vector
    tempest::core::span<int> s(v);

    // create a subspan from the span
    tempest::core::span<int, 5> sub = s.last<5>();

    // check the size of the subspan
    EXPECT_EQ(sub.size(), 5);

    // check the values of the subspan
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(sub[i], i + 5);
    }
}