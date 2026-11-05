#include <tempest/vector.hpp>

#include <gtest/gtest.h>

#include <tempest/iterator.hpp>
#include <tempest/limits.hpp>
#include <tempest/string.hpp>
#include <tempest/utility.hpp>

template <typename T>
using vector = tempest::vector<T>;

TEST(vector, default_constructor)
{
    vector<int> v;
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 0);
}

TEST(vector, constructor_with_size)
{
    vector<int> v(10);
    EXPECT_EQ(v.size(), 10);
    EXPECT_EQ(v.capacity(), 10);
}

TEST(vector, constructor_with_size_and_value)
{
    vector<int> v(10, 42);
    EXPECT_EQ(v.size(), 10);
    EXPECT_EQ(v.capacity(), 10);
    for (const auto& i : v)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(vector, copy_constructor)
{
    vector<int> v1(10, 42);
    vector<int> v2(v1);
    EXPECT_EQ(v2.size(), 10);
    EXPECT_GE(v2.capacity(), 10);
    for (const auto& i : v2)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(vector, copy_constructor_non_trivial_copy)
{
    struct non_trivial
    {
        int i;

        non_trivial(int i) : i(i)
        {
        }
    };

    vector<non_trivial> v1(10, 42);
    vector<non_trivial> v2(v1);
    EXPECT_EQ(v2.size(), 10);
    EXPECT_GE(v2.capacity(), 10);
    for (const auto& i : v2)
    {
        EXPECT_EQ(i.i, 42);
    }
}

TEST(vector, move_constructor)
{
    vector<int> v1(10, 42);
    vector<int> v2(tempest::move(v1));
    EXPECT_EQ(v2.size(), 10);
    EXPECT_EQ(v2.capacity(), 10);
    for (const auto& i : v2)
    {
        EXPECT_EQ(i, 42);
    }

    EXPECT_EQ(v1.size(), 0);
    EXPECT_EQ(v1.capacity(), 0);
}

TEST(vector, variadic_constructor)
{
    vector<int> v(tempest::init_list, 1, 2, 3, 4, 5);
    EXPECT_EQ(v.size(), 5);
    EXPECT_GE(v.capacity(), 5);
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v[i], i + 1);
    }
}

TEST(vector, copy_assignment)
{
    vector<int> v1(10, 42);
    vector<int> v2;
    v2 = v1;
    EXPECT_EQ(v2.size(), 10);
    EXPECT_GE(v2.capacity(), 10);
    for (const auto& i : v2)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(vector, move_assignment)
{
    vector<int> v1(10, 42);
    vector<int> v2;
    v2 = tempest::move(v1);
    EXPECT_EQ(v2.size(), 10);
    EXPECT_EQ(v2.capacity(), 10);
    for (const auto& i : v2)
    {
        EXPECT_EQ(i, 42);
    }

    EXPECT_EQ(v1.size(), 0);
    EXPECT_EQ(v1.capacity(), 0);
}

TEST(vector, push_back)
{
    vector<int> v;
    for (int i = 0; i < 10; ++i)
    {
        v.push_back(i);
    }
    EXPECT_EQ(v.size(), 10);
    EXPECT_GE(v.capacity(), 10);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(v[i], i);
    }
}

TEST(vector, pop_back)
{
    vector<int> v(10, 42);
    for (int i = 0; i < 5; ++i)
    {
        v.pop_back();
    }
    EXPECT_EQ(v.size(), 5);
    EXPECT_EQ(v.capacity(), 10);
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
}

TEST(vector, clear)
{
    vector<int> v(10, 42);
    v.clear();
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 10);
}

TEST(vector, resize)
{
    vector<int> v(10, 42);
    v.resize(5);
    EXPECT_EQ(v.size(), 5);
    EXPECT_EQ(v.capacity(), 10);
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
}

TEST(vector, reserve)
{
    vector<int> v;
    v.reserve(10);
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 10);
}

TEST(vector, shrink_to_fit)
{
    vector<int> v(10, 42);
    v.resize(5);
    v.shrink_to_fit();
    EXPECT_EQ(v.size(), 5);
    EXPECT_EQ(v.capacity(), 5);
}

TEST(vector, at)
{
    vector<int> v(10, 42);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(v.at(i), 42);
    }
}

TEST(vector, operator_brackets)
{
    vector<int> v(10, 42);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
}

TEST(vector, front)
{
    vector<int> v(10, 42);
    EXPECT_EQ(v.front(), 42);
}

TEST(vector, back)
{
    vector<int> v(10, 42);
    EXPECT_EQ(v.back(), 42);
}

TEST(vector, data)
{
    vector<int> v(10, 42);
    int* data = v.data();
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(data[i], 42);
    }
}

TEST(vector, begin_end)
{
    vector<int> v(10, 42);
    int i = 0;
    for (int & it : v)
    {
        EXPECT_EQ(it, 42);
        ++i;
    }
    EXPECT_EQ(i, 10);
}

TEST(vector, cbegin_cend)
{
    vector<int> v(10, 42);
    int i = 0;
    for (int it : v)
    {
        EXPECT_EQ(it, 42);
        ++i;
    }
    EXPECT_EQ(i, 10);
}

// NOLINTBEGIN(modernize-loop-convert)
TEST(vector, rbegin_rend)
{
    vector<int> v(10, 42);
    int i = 0;
    for (auto it = v.rbegin(); it != v.rend(); ++it)
    {
        EXPECT_EQ(*it, 42);
        ++i;
    }
    EXPECT_EQ(i, 10);
}

TEST(vector, crbegin_crend)
{
    vector<int> v(10, 42);
    int i = 0;
    for (auto it = v.crbegin(); it != v.crend(); ++it)
    {
        EXPECT_EQ(*it, 42);
        ++i;
    }
    EXPECT_EQ(i, 10);
}
// NOLINTEND(modernize-loop-convert)

TEST(vector, insert)
{
    vector<int> v(10, 42);
    auto *it = v.insert(v.begin() + 5, 24);
    EXPECT_EQ(v.size(), 11);
    EXPECT_GE(v.capacity(), v.size());
    EXPECT_EQ(*it, 24);
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
    EXPECT_EQ(v[5], 24);
    for (int i = 6; i < 11; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
}

/// @brief Verifies that self-referential vector::insert (inserting an existing element from the vector)
/// does not cause use-after-free, dangling references, or moved-from value corruption, both when
/// capacity is exceeded (triggering reallocation) and when capacity is pre-reserved.
TEST(vector, vector_insert_self_referential)
{
    // =========================================================================
    // Case 1: Trivial Type (int) with Reallocation Triggered (Capacity Exceeded)
    // =========================================================================
    {
        // Setup: Vector initialized with exact capacity matching size
        auto vec = vector<int>{tempest::init_list, 10, 20, 30};
        vec.shrink_to_fit();
        EXPECT_EQ(vec.size(), 3);
        EXPECT_EQ(vec.capacity(), 3);

        // Act: Insert vec[0] into vec.begin() + 1 when size == capacity
        const auto it_first = vec.insert(vec.begin() + 1, vec[0]);

        // Assert: Value is intact, capacity grew, and elements are in expected order
        EXPECT_EQ(vec.size(), 4);
        EXPECT_GE(vec.capacity(), 4);
        EXPECT_EQ(*it_first, 10);
        EXPECT_EQ(vec[0], 10);
        EXPECT_EQ(vec[1], 10);
        EXPECT_EQ(vec[2], 20);
        EXPECT_EQ(vec[3], 30);

        // Act: Shrink to fit then insert vec.back() into vec.begin() + 1 triggering another reallocation
        vec.shrink_to_fit();
        EXPECT_EQ(vec.capacity(), 4);
        const auto it_back = vec.insert(vec.begin() + 1, vec.back());

        // Assert: Value (30) correctly inserted without dangling read
        EXPECT_EQ(vec.size(), 5);
        EXPECT_GE(vec.capacity(), 5);
        EXPECT_EQ(*it_back, 30);
        EXPECT_EQ(vec[0], 10);
        EXPECT_EQ(vec[1], 30);
        EXPECT_EQ(vec[2], 10);
        EXPECT_EQ(vec[3], 20);
        EXPECT_EQ(vec[4], 30);
    }

    // =========================================================================
    // Case 2: Trivial Type (int) with Pre-Reserved Capacity (No Reallocation)
    // =========================================================================
    {
        // Setup: Vector with pre-reserved excess capacity
        auto vec = vector<int>{};
        vec.reserve(16);
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);
        EXPECT_EQ(vec.size(), 3);
        EXPECT_GE(vec.capacity(), 16);

        // Act: Insert vec[0] into vec.begin() + 1 with sufficient capacity
        const auto it_first = vec.insert(vec.begin() + 1, vec[0]);

        // Assert: All elements shifted and inserted correctly without moving-from source element
        EXPECT_EQ(vec.size(), 4);
        EXPECT_EQ(*it_first, 10);
        EXPECT_EQ(vec[0], 10);
        EXPECT_EQ(vec[1], 10);
        EXPECT_EQ(vec[2], 20);
        EXPECT_EQ(vec[3], 30);

        // Act: Insert vec.back() into vec.begin() + 1
        const auto it_back = vec.insert(vec.begin() + 1, vec.back());

        // Assert: All elements intact
        EXPECT_EQ(vec.size(), 5);
        EXPECT_EQ(*it_back, 30);
        EXPECT_EQ(vec[0], 10);
        EXPECT_EQ(vec[1], 30);
        EXPECT_EQ(vec[2], 10);
        EXPECT_EQ(vec[3], 20);
        EXPECT_EQ(vec[4], 30);
    }

    // =========================================================================
    // Case 3: Non-Trivial Copy/Move Type with Reallocation & Pre-Reserved
    // =========================================================================
    {
        struct non_trivial_item
        {
            int value{0};
            bool moved_from{false};

            non_trivial_item() = default;
            explicit non_trivial_item(int val) : value{val} {}

            non_trivial_item(const non_trivial_item& other)
                : value{other.value}, moved_from{other.moved_from}
            {
            }

            non_trivial_item(non_trivial_item&& other) noexcept
                : value{other.value}, moved_from{false}
            {
                other.moved_from = true;
                other.value = -1;
            }

            auto operator=(const non_trivial_item& other) -> non_trivial_item&
            {
                if (this != &other)
                {
                    value = other.value;
                    moved_from = other.moved_from;
                }
                return *this;
            }

            auto operator=(non_trivial_item&& other) noexcept -> non_trivial_item&
            {
                if (this != &other)
                {
                    value = other.value;
                    moved_from = false;
                    other.moved_from = true;
                    other.value = -1;
                }
                return *this;
            }
        };

        // Subcase A: Reallocation triggered
        {
            auto vec = vector<non_trivial_item>{};
            vec.push_back(non_trivial_item{100});
            vec.push_back(non_trivial_item{200});
            vec.push_back(non_trivial_item{300});
            vec.shrink_to_fit();
            EXPECT_EQ(vec.size(), 3);
            EXPECT_EQ(vec.capacity(), 3);

            // Insert vec[0] into vec.begin() + 1
            const auto it_first = vec.insert(vec.begin() + 1, vec[0]);
            EXPECT_EQ(vec.size(), 4);
            EXPECT_EQ(it_first->value, 100);
            EXPECT_FALSE(it_first->moved_from);
            EXPECT_EQ(vec[0].value, 100);
            EXPECT_FALSE(vec[0].moved_from);
            EXPECT_EQ(vec[1].value, 100);
            EXPECT_FALSE(vec[1].moved_from);
            EXPECT_EQ(vec[2].value, 200);
            EXPECT_FALSE(vec[2].moved_from);
            EXPECT_EQ(vec[3].value, 300);
            EXPECT_FALSE(vec[3].moved_from);

            // Insert vec.back() into vec.begin() + 1 with reallocation
            vec.shrink_to_fit();
            const auto it_back = vec.insert(vec.begin() + 1, vec.back());
            EXPECT_EQ(vec.size(), 5);
            EXPECT_EQ(it_back->value, 300);
            EXPECT_FALSE(it_back->moved_from);
            EXPECT_EQ(vec[0].value, 100);
            EXPECT_EQ(vec[1].value, 300);
            EXPECT_EQ(vec[2].value, 100);
            EXPECT_EQ(vec[3].value, 200);
            EXPECT_EQ(vec[4].value, 300);
            for (const auto& item : vec)
            {
                EXPECT_FALSE(item.moved_from);
            }
        }

        // Subcase B: Pre-reserved capacity (no reallocation)
        {
            auto vec = vector<non_trivial_item>{};
            vec.reserve(16);
            vec.push_back(non_trivial_item{100});
            vec.push_back(non_trivial_item{200});
            vec.push_back(non_trivial_item{300});

            // Insert vec[0] into vec.begin() + 1
            const auto it_first = vec.insert(vec.begin() + 1, vec[0]);
            EXPECT_EQ(vec.size(), 4);
            EXPECT_EQ(it_first->value, 100);
            EXPECT_FALSE(it_first->moved_from);
            EXPECT_EQ(vec[0].value, 100);
            EXPECT_EQ(vec[1].value, 100);
            EXPECT_EQ(vec[2].value, 200);
            EXPECT_EQ(vec[3].value, 300);

            // Insert vec.back() into vec.begin() + 1
            const auto it_back = vec.insert(vec.begin() + 1, vec.back());
            EXPECT_EQ(vec.size(), 5);
            EXPECT_EQ(it_back->value, 300);
            EXPECT_FALSE(it_back->moved_from);
            EXPECT_EQ(vec[0].value, 100);
            EXPECT_EQ(vec[1].value, 300);
            EXPECT_EQ(vec[2].value, 100);
            EXPECT_EQ(vec[3].value, 200);
            EXPECT_EQ(vec[4].value, 300);
            for (const auto& item : vec)
            {
                EXPECT_FALSE(item.moved_from);
            }
        }
    }
}

TEST(vector, erase)
{
    vector<int> v(10, 42);
    (void)v.erase(v.begin() + 5);
    EXPECT_EQ(v.size(), 9);
    EXPECT_GE(v.capacity(), v.size());
    
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
    
    for (int i = 5; i < 9; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
}

TEST(vector, erase_range)
{
    vector<int> v(10, 42);
    (void)v.erase(v.begin() + 5, v.begin() + 7);
    EXPECT_EQ(v.size(), 8);
    EXPECT_GE(v.capacity(), v.size());
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
    for (int i = 5; i < 8; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
}

TEST(vector, swap)
{
    vector<int> v1(10, 42);
    vector<int> v2(5, 24);
    v1.swap(v2);
    EXPECT_EQ(v1.size(), 5);
    EXPECT_GE(v1.capacity(), v1.size());
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v1[i], 24);
    }
    EXPECT_EQ(v2.size(), 10);
    EXPECT_GE(v2.capacity(), 10);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(v2[i], 42);
    }
}

TEST(vector, operator_equal)
{
    vector<int> v1(10, 42);
    vector<int> v2(10, 42);
    EXPECT_TRUE(v1 == v2);
    v2[5] = 24;
    EXPECT_FALSE(v1 == v2);
}

TEST(vector, operator_not_equal)
{
    vector<int> v1(10, 42);
    vector<int> v2(10, 42);
    EXPECT_FALSE(v1 != v2);
    v2[5] = 24;
    EXPECT_TRUE(v1 != v2);
}

TEST(vector, operator_less)
{
    vector<int> v1(10, 42);
    vector<int> v2(10, 42);
    EXPECT_FALSE(v1 < v2);
    v1[5] = 24;
    EXPECT_TRUE(v1 < v2);
}

TEST(vector, operator_less_or_equal)
{
    vector<int> v1(10, 42);
    vector<int> v2(10, 42);
    EXPECT_TRUE(v1 <= v2);
    v1[5] = 24;
    EXPECT_TRUE(v1 <= v2);
    v2[5] = 42;
    EXPECT_TRUE(v1 <= v2);
}

TEST(vector, operator_greater)
{
    vector<int> v1(10, 42);
    vector<int> v2(10, 42);
    EXPECT_FALSE(v1 > v2);
    v1[5] = 24;
    EXPECT_FALSE(v1 > v2);
    v2[5] = 42;
    EXPECT_FALSE(v1 > v2);
}

TEST(vector, operator_greater_or_equal)
{
    vector<int> v1(10, 42);
    vector<int> v2(10, 42);
    EXPECT_TRUE(v1 >= v2);
    v1[5] = 24;
    EXPECT_FALSE(v1 >= v2);
    v2[5] = 42;
    EXPECT_FALSE(v1 >= v2);
}

TEST(vector, swap_non_member)
{
    vector<int> v1(10, 42);
    vector<int> v2(5, 24);
    swap(v1, v2);
    EXPECT_EQ(v1.size(), 5);
    EXPECT_GE(v1.capacity(), v1.size());
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v1[i], 24);
    }
    EXPECT_EQ(v2.size(), 10);
    EXPECT_GE(v2.capacity(), 10);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(v2[i], 42);
    }
}

TEST(vector, iterator_checks)
{
    using vec_it = tempest::vector<int>::iterator;
    EXPECT_TRUE(tempest::contiguous_iterator<vec_it>);
}

/// @brief Verifies that calling reserve with capacity exceeding host memory fails safely
/// without corrupting capacity, data pointers, or crashing subsequent push_back operations.
TEST(vector, reserve_huge_allocation_failure_resilience)
{
    // Setup: empty vector
    auto vec = tempest::vector<tempest::string>{};
    const auto huge_cap = static_cast<size_t>(-1) / sizeof(tempest::string);

    // Act: attempt to reserve huge capacity
    vec.reserve(huge_cap);

    // Assert: vector remains safely empty and uncorrupted
    EXPECT_EQ(vec.capacity(), 0U);
    EXPECT_EQ(vec.size(), 0U);
    EXPECT_EQ(vec.data(), nullptr);

    // Act: push element should successfully allocate normal capacity and not dereference null
    vec.push_back("safe_string");

    // Assert: element is properly stored
    EXPECT_EQ(vec.size(), 1U);
    EXPECT_GE(vec.capacity(), 1U);
    EXPECT_EQ(vec[0], "safe_string");
}

/// @brief Verifies that calling reserve with huge capacity on a non-empty vector preserves
/// all existing elements and original capacity without data loss or memory corruption.
TEST(vector, reserve_huge_with_existing_elements_preserves_content)
{
    // Setup: vector with existing elements
    auto vec = tempest::vector<tempest::string>{};
    vec.push_back("first");
    vec.push_back("second");
    const auto original_cap = vec.capacity();

    // Act: attempt huge reservation
    const auto huge_cap = static_cast<size_t>(-1) / sizeof(tempest::string);
    vec.reserve(huge_cap);

    // Assert: existing elements and original capacity are completely preserved
    EXPECT_EQ(vec.capacity(), original_cap);
    EXPECT_EQ(vec.size(), 2U);
    EXPECT_EQ(vec[0], "first");
    EXPECT_EQ(vec[1], "second");
}

/// @brief Verifies that calling reserve with value exceeding max_size() is rejected safely.
TEST(vector, reserve_overflow_above_max_size_is_rejected)
{
    // Setup: vector of ints
    auto vec = tempest::vector<int>{};
    const auto overflow_cap = static_cast<size_t>(-1);

    // Act: reserve value that exceeds max_size()
    vec.reserve(overflow_cap);

    // Assert: rejected without wrapping or corrupting state
    EXPECT_EQ(vec.capacity(), 0U);
    EXPECT_EQ(vec.size(), 0U);
    EXPECT_EQ(vec.data(), nullptr);
}

namespace
{
    template <typename T>
    struct mock_failing_allocator
    {
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        bool* fail_switch = nullptr;

        constexpr mock_failing_allocator() noexcept = default;
        constexpr explicit mock_failing_allocator(bool* failure_flag) noexcept : fail_switch{failure_flag}
        {
        }

        template <typename U>
        constexpr mock_failing_allocator(const mock_failing_allocator<U>& other) noexcept : fail_switch{other.fail_switch}
        {
        }

        [[nodiscard]] auto allocate(size_t count) -> T*
        {
            if (fail_switch != nullptr && *fail_switch)
            {
                return nullptr;
            }
            if (sizeof(T) != 0 && count > (tempest::numeric_limits<size_t>::max() / sizeof(T)))
            {
                return nullptr;
            }
            auto* memory_buffer = tempest::aligned_alloc(sizeof(T) * count, alignof(T));
            return static_cast<T*>(memory_buffer);
        }

        void deallocate(T* pointer, [[maybe_unused]] size_t count)
        {
            tempest::aligned_free(pointer);
        }
    };

    template <typename T, typename U>
    [[nodiscard]] constexpr auto operator==(const mock_failing_allocator<T>& lhs,
                                            const mock_failing_allocator<U>& rhs) noexcept -> bool
    {
        return lhs.fail_switch == rhs.fail_switch;
    }
} // namespace

/// @brief Verifies that calling reserve with boundary values approaching and exceeding
/// max_size() on empty vectors leaves them safely empty without null dereference.
TEST(vector, empirical_challenge_reserve_boundary_stress_empty)
{
    // Setup: empty vectors of different element types
    auto int_vector = tempest::vector<int>{};
    auto string_vector = tempest::vector<tempest::string>{};
    auto uint64_vector = tempest::vector<uint64_t>{};

    const auto max_int_cap = tempest::numeric_limits<size_t>::max() / sizeof(int);
    const auto max_string_cap = tempest::numeric_limits<size_t>::max() / sizeof(tempest::string);
    const auto max_uint64_cap = tempest::numeric_limits<size_t>::max() / sizeof(uint64_t);

    // Act: reserve with extreme boundary values on empty int vector
    int_vector.reserve(tempest::numeric_limits<size_t>::max());
    EXPECT_EQ(int_vector.capacity(), 0U);
    EXPECT_EQ(int_vector.size(), 0U);
    EXPECT_EQ(int_vector.data(), nullptr);

    int_vector.reserve(max_int_cap + 1);
    EXPECT_EQ(int_vector.capacity(), 0U);
    EXPECT_EQ(int_vector.size(), 0U);
    EXPECT_EQ(int_vector.data(), nullptr);

    int_vector.reserve(max_int_cap);
    EXPECT_EQ(int_vector.capacity(), 0U);
    EXPECT_EQ(int_vector.size(), 0U);
    EXPECT_EQ(int_vector.data(), nullptr);

    int_vector.reserve(max_int_cap - 1);
    EXPECT_EQ(int_vector.capacity(), 0U);
    EXPECT_EQ(int_vector.size(), 0U);
    EXPECT_EQ(int_vector.data(), nullptr);

    int_vector.reserve(1ULL << 62);
    EXPECT_EQ(int_vector.capacity(), 0U);
    EXPECT_EQ(int_vector.size(), 0U);
    EXPECT_EQ(int_vector.data(), nullptr);

    // Act: reserve on empty string vector
    string_vector.reserve(tempest::numeric_limits<size_t>::max());
    EXPECT_EQ(string_vector.capacity(), 0U);
    EXPECT_EQ(string_vector.size(), 0U);
    EXPECT_EQ(string_vector.data(), nullptr);

    string_vector.reserve(max_string_cap);
    EXPECT_EQ(string_vector.capacity(), 0U);
    EXPECT_EQ(string_vector.size(), 0U);
    EXPECT_EQ(string_vector.data(), nullptr);

    // Act: reserve on empty uint64 vector
    uint64_vector.reserve(max_uint64_cap);
    EXPECT_EQ(uint64_vector.capacity(), 0U);
    EXPECT_EQ(uint64_vector.size(), 0U);
    EXPECT_EQ(uint64_vector.data(), nullptr);

    // Act: push_back and emplace_back on vectors after failed reserve operations
    int_vector.push_back(42);
    EXPECT_EQ(int_vector.size(), 1U);
    EXPECT_GE(int_vector.capacity(), 1U);
    EXPECT_NE(int_vector.data(), nullptr);
    EXPECT_EQ(int_vector[0], 42);

    int_vector.emplace_back(84);
    EXPECT_EQ(int_vector.size(), 2U);
    EXPECT_GE(int_vector.capacity(), 2U);
    EXPECT_EQ(int_vector[1], 84);
    EXPECT_EQ(int_vector.front(), 42);
    EXPECT_EQ(int_vector.back(), 84);

    string_vector.push_back("first_entry");
    string_vector.emplace_back("second_entry");
    EXPECT_EQ(string_vector.size(), 2U);
    EXPECT_GE(string_vector.capacity(), 2U);
    EXPECT_EQ(string_vector[0], "first_entry");
    EXPECT_EQ(string_vector[1], "second_entry");
}

/// @brief Verifies that calling reserve with extreme capacities on populated vectors preserves
/// all existing elements, original capacity, and pointer identity without memory corruption.
TEST(vector, empirical_challenge_reserve_boundary_stress_populated)
{
    // Setup: populated vector of strings and ints
    auto populated_string_vec = tempest::vector<tempest::string>{};
    populated_string_vec.push_back("alpha");
    populated_string_vec.push_back("beta");
    populated_string_vec.push_back("gamma");
    populated_string_vec.push_back("delta");
    populated_string_vec.push_back("epsilon");

    const auto original_string_cap = populated_string_vec.capacity();
    const auto* original_string_ptr = populated_string_vec.data();
    const auto max_string_cap = tempest::numeric_limits<size_t>::max() / sizeof(tempest::string);

    // Act: stress reserve with multiple extreme capacities
    populated_string_vec.reserve(tempest::numeric_limits<size_t>::max());
    EXPECT_EQ(populated_string_vec.capacity(), original_string_cap);
    EXPECT_EQ(populated_string_vec.size(), 5U);
    EXPECT_EQ(populated_string_vec.data(), original_string_ptr);

    populated_string_vec.reserve(max_string_cap + 1);
    EXPECT_EQ(populated_string_vec.capacity(), original_string_cap);
    EXPECT_EQ(populated_string_vec.size(), 5U);
    EXPECT_EQ(populated_string_vec.data(), original_string_ptr);

    populated_string_vec.reserve(max_string_cap);
    EXPECT_EQ(populated_string_vec.capacity(), original_string_cap);
    EXPECT_EQ(populated_string_vec.size(), 5U);
    EXPECT_EQ(populated_string_vec.data(), original_string_ptr);

    populated_string_vec.reserve(1ULL << 60);
    EXPECT_EQ(populated_string_vec.capacity(), original_string_cap);
    EXPECT_EQ(populated_string_vec.size(), 5U);
    EXPECT_EQ(populated_string_vec.data(), original_string_ptr);

    // Assert: content integrity
    EXPECT_EQ(populated_string_vec[0], "alpha");
    EXPECT_EQ(populated_string_vec[1], "beta");
    EXPECT_EQ(populated_string_vec[2], "gamma");
    EXPECT_EQ(populated_string_vec[3], "delta");
    EXPECT_EQ(populated_string_vec[4], "epsilon");

    // Setup: populated vector of ints
    auto populated_int_vec = tempest::vector<int>{};
    for (auto val = 0; val < 16; ++val)
    {
        populated_int_vec.push_back(val * 10);
    }
    const auto original_int_cap = populated_int_vec.capacity();
    const auto* original_int_ptr = populated_int_vec.data();
    const auto max_int_cap = tempest::numeric_limits<size_t>::max() / sizeof(int);

    // Act: stress reserve
    populated_int_vec.reserve(tempest::numeric_limits<size_t>::max());
    populated_int_vec.reserve(max_int_cap);
    populated_int_vec.reserve(max_int_cap - 1);

    // Assert: int vector preserved
    EXPECT_EQ(populated_int_vec.capacity(), original_int_cap);
    EXPECT_EQ(populated_int_vec.size(), 16U);
    EXPECT_EQ(populated_int_vec.data(), original_int_ptr);
    for (auto val = 0; val < 16; ++val)
    {
        EXPECT_EQ(populated_int_vec[static_cast<size_t>(val)], val * 10);
    }
}

/// @brief Verifies that subsequent container operations (clear, shrink_to_fit, insert, erase,
/// operator[]) function correctly after simulated allocation failures on reserve.
TEST(vector, empirical_challenge_subsequent_container_operations_after_reserve_failure)
{
    // Setup: populated vector of strings
    auto string_vec = tempest::vector<tempest::string>{};
    string_vec.push_back("zero");
    string_vec.push_back("one");
    string_vec.push_back("two");

    const auto max_string_cap = tempest::numeric_limits<size_t>::max() / sizeof(tempest::string);

    // Act: trigger failed reserve
    string_vec.reserve(max_string_cap);

    // Assert: element mutation via operator[] and at()
    string_vec[1] = "ONE_MODIFIED";
    EXPECT_EQ(string_vec[1], "ONE_MODIFIED");
    EXPECT_EQ(string_vec.at(1), "ONE_MODIFIED");

    // Act: insert element at position 1
    const auto insert_iter = string_vec.insert(string_vec.begin() + 1, "inserted_between");
    EXPECT_EQ(string_vec.size(), 4U);
    EXPECT_EQ(*insert_iter, "inserted_between");
    EXPECT_EQ(string_vec[0], "zero");
    EXPECT_EQ(string_vec[1], "inserted_between");
    EXPECT_EQ(string_vec[2], "ONE_MODIFIED");
    EXPECT_EQ(string_vec[3], "two");

    // Act: erase element at position 2
    const auto erase_iter = string_vec.erase(string_vec.begin() + 2);
    EXPECT_EQ(string_vec.size(), 3U);
    EXPECT_EQ(*erase_iter, "two");
    EXPECT_EQ(string_vec[0], "zero");
    EXPECT_EQ(string_vec[1], "inserted_between");
    EXPECT_EQ(string_vec[2], "two");

    // Act: shrink_to_fit adjusts capacity to size
    string_vec.shrink_to_fit();
    EXPECT_EQ(string_vec.capacity(), 3U);
    EXPECT_EQ(string_vec.size(), 3U);
    EXPECT_EQ(string_vec[0], "zero");
    EXPECT_EQ(string_vec[1], "inserted_between");
    EXPECT_EQ(string_vec[2], "two");

    // Act: clear empties container while retaining capacity
    string_vec.clear();
    EXPECT_EQ(string_vec.size(), 0U);
    EXPECT_TRUE(string_vec.empty());
    EXPECT_EQ(string_vec.capacity(), 3U);

    // Act: shrink_to_fit on empty container releases storage
    string_vec.shrink_to_fit();
    EXPECT_EQ(string_vec.size(), 0U);
    EXPECT_EQ(string_vec.capacity(), 0U);
    EXPECT_EQ(string_vec.data(), nullptr);

    // Act: push_back and emplace_back on freshly shrunk container
    string_vec.push_back("fresh_first");
    string_vec.emplace_back("fresh_second");
    EXPECT_EQ(string_vec.size(), 2U);
    EXPECT_GE(string_vec.capacity(), 2U);
    EXPECT_EQ(string_vec[0], "fresh_first");
    EXPECT_EQ(string_vec[1], "fresh_second");
}

/// @brief Verifies container invariants and allocation failure resilience using a mock allocator
/// that can be dynamically instructed to fail on demand.
TEST(vector, empirical_challenge_controlled_failing_allocator_resilience)
{
    // Setup: controlled failure flag
    auto fail_allocations = false;
    auto alloc_instance = mock_failing_allocator<int>{&fail_allocations};
    auto test_vector = tempest::vector<int, mock_failing_allocator<int>>{alloc_instance};

    // Act: fail allocation on empty vector reserve
    fail_allocations = true;
    test_vector.reserve(64);

    // Assert: empty vector remains untouched
    EXPECT_EQ(test_vector.capacity(), 0U);
    EXPECT_EQ(test_vector.size(), 0U);
    EXPECT_EQ(test_vector.data(), nullptr);

    // Act: re-enable allocation and populate container
    fail_allocations = false;
    test_vector.reserve(8);
    EXPECT_GE(test_vector.capacity(), 8U);
    for (auto val = 0; val < 5; ++val)
    {
        test_vector.push_back(val * 100);
    }
    const auto baseline_capacity = test_vector.capacity();
    const auto* baseline_data = test_vector.data();

    // Act: simulate allocation failure on reserve for populated vector
    fail_allocations = true;
    test_vector.reserve(baseline_capacity * 4);

    // Assert: original state preserved
    EXPECT_EQ(test_vector.capacity(), baseline_capacity);
    EXPECT_EQ(test_vector.size(), 5U);
    EXPECT_EQ(test_vector.data(), baseline_data);
    for (auto val = 0; val < 5; ++val)
    {
        EXPECT_EQ(test_vector[static_cast<size_t>(val)], val * 100);
    }

    // Act: simulate allocation failure on shrink_to_fit
    test_vector.shrink_to_fit();
    EXPECT_EQ(test_vector.capacity(), baseline_capacity);
    EXPECT_EQ(test_vector.size(), 5U);
    EXPECT_EQ(test_vector.data(), baseline_data);

    // Act: insert within remaining capacity succeeds even when allocation fails
    const auto insert_result = test_vector.insert(test_vector.begin() + 2, 9999);
    EXPECT_NE(insert_result, test_vector.end());
    EXPECT_EQ(test_vector.size(), 6U);
    EXPECT_EQ(test_vector[2], 9999);

    // Fill remaining capacity to boundary (capacity 8)
    test_vector.push_back(1000);
    test_vector.push_back(2000);
    EXPECT_EQ(test_vector.size(), 8U);
    EXPECT_EQ(test_vector.capacity(), 8U);

    // Act: insert requiring reallocation when allocator fails
    const auto failed_insert_result = test_vector.insert(test_vector.begin() + 1, 8888);
    EXPECT_EQ(failed_insert_result, test_vector.end());
    EXPECT_EQ(test_vector.size(), 8U);
    EXPECT_EQ(test_vector.capacity(), 8U);

    // Act: restore allocator and verify push_back recovers
    fail_allocations = false;
    test_vector.push_back(3000);
    EXPECT_EQ(test_vector.size(), 9U);
    EXPECT_GE(test_vector.capacity(), 9U);
    EXPECT_EQ(test_vector.back(), 3000);
}