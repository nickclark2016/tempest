#include <gtest/gtest.h>
#include <tempest/deque.hpp>

TEST(deque_test, construct_and_size) {
    // Arrange
    tempest::deque<int> d;

    // Act
    const bool is_empty = d.empty();
    const auto size = d.size();

    // Assert
    EXPECT_TRUE(is_empty);
    EXPECT_EQ(size, 0);
}

TEST(deque_test, push_back_and_access) {
    // Arrange
    tempest::deque<int> d;

    // Act
    for (int i = 0; i < 10000; ++i) {
        d.push_back(i);
    }

    // Assert
    EXPECT_EQ(d.size(), 10000);
    EXPECT_FALSE(d.empty());

    for (int i = 0; i < 10000; ++i) {
        EXPECT_EQ(d[i], i);
    }
}

TEST(deque_test, push_front_and_access) {
    // Arrange
    tempest::deque<int> d;

    // Act
    for (int i = 0; i < 10000; ++i) {
        d.push_front(i);
    }

    // Assert
    EXPECT_EQ(d.size(), 10000);

    for (int i = 0; i < 10000; ++i) {
        EXPECT_EQ(d[i], 9999 - i);
    }
}

TEST(deque_test, mixed_push_and_pop) {
    // Arrange
    tempest::deque<int> d;

    // Act
    d.push_back(1);
    d.push_back(2);
    d.push_front(0);

    // Assert
    EXPECT_EQ(d.size(), 3);
    EXPECT_EQ(d[0], 0);
    EXPECT_EQ(d[1], 1);
    EXPECT_EQ(d[2], 2);

    // Act
    d.pop_back();

    // Assert
    EXPECT_EQ(d.size(), 2);
    EXPECT_EQ(d.back(), 1);

    // Act
    d.pop_front();

    // Assert
    EXPECT_EQ(d.size(), 1);
    EXPECT_EQ(d.front(), 1);
}

TEST(deque_test, iterators) {
    // Arrange
    tempest::deque<int> d;
    for (int i = 0; i < 5; ++i) {
        d.push_back(i);
    }

    // Act & Assert
    int expected = 0;
    for (auto it = d.begin(); it != d.end(); ++it) {
        EXPECT_EQ(*it, expected++);
    }
    EXPECT_EQ(expected, 5);

    expected = 4;
    for (auto it = d.rbegin(); it != d.rend(); ++it) {
        EXPECT_EQ(*it, expected--);
    }
}

TEST(deque_test, copy_construction) {
    // Arrange
    tempest::deque<int> d1;
    d1.push_back(1);
    d1.push_back(2);

    // Act
    tempest::deque<int> d2 = d1;

    // Assert
    EXPECT_EQ(d2.size(), 2);
    EXPECT_EQ(d2[0], 1);
    EXPECT_EQ(d2[1], 2);

    // Act
    d2[0] = 10;

    // Assert
    EXPECT_EQ(d1[0], 1);
    EXPECT_EQ(d2[0], 10);
}

TEST(deque_test, move_construction) {
    // Arrange
    tempest::deque<int> d1;
    d1.push_back(1);
    d1.push_back(2);

    // Act
    tempest::deque<int> d2 = tempest::move(d1);

    // Assert
    EXPECT_EQ(d2.size(), 2);
    EXPECT_EQ(d2[0], 1);
    EXPECT_EQ(d2[1], 2);
    EXPECT_EQ(d1.size(), 0);
}

TEST(deque_test, clear) {
    // Arrange
    tempest::deque<int> d;
    d.push_back(1);
    d.push_back(2);
    d.push_back(3);

    // Act
    d.clear();

    // Assert
    EXPECT_EQ(d.size(), 0);
    EXPECT_TRUE(d.empty());
}

TEST(deque_test, copy_assignment) {
    // Arrange
    tempest::deque<int> d1;
    d1.push_back(1);
    d1.push_back(2);
    tempest::deque<int> d2;
    d2.push_back(5);

    // Act
    d2 = d1;

    // Assert
    EXPECT_EQ(d2.size(), 2);
    EXPECT_EQ(d2[0], 1);
    EXPECT_EQ(d2[1], 2);
    EXPECT_EQ(d1.size(), 2);
}

TEST(deque_test, move_assignment) {
    // Arrange
    tempest::deque<int> d1;
    d1.push_back(1);
    d1.push_back(2);
    tempest::deque<int> d2;
    d2.push_back(5);

    // Act
    d2 = tempest::move(d1);

    // Assert
    EXPECT_EQ(d2.size(), 2);
    EXPECT_EQ(d2[0], 1);
    EXPECT_EQ(d2[1], 2);
    EXPECT_EQ(d1.size(), 0);
}

TEST(deque_test, iterator_arithmetic) {
    // Arrange
    tempest::deque<int> d;
    for (int i = 0; i < 10; ++i) {
        d.push_back(i);
    }

    // Act
    auto it = d.begin();
    auto it2 = it + 5;
    auto it3 = it2 - 2;

    // Assert
    EXPECT_EQ(*it, 0);
    EXPECT_EQ(*it2, 5);
    EXPECT_EQ(*it3, 3);
    EXPECT_EQ(it2 - it, 5);

    // Act
    auto element = it[4];

    // Assert
    EXPECT_EQ(element, 4);

    // Act
    it += 3;

    // Assert
    EXPECT_EQ(*it, 3);

    // Act
    it -= 1;

    // Assert
    EXPECT_EQ(*it, 2);
}

TEST(deque_test, iterator_comparisons) {
    // Arrange
    tempest::deque<int> d;
    for (int i = 0; i < 5; ++i) {
        d.push_back(i);
    }

    // Act
    auto it1 = d.begin();
    auto it2 = d.begin() + 2;
    auto it3 = d.begin() + 2;

    // Assert
    EXPECT_TRUE(it1 != it2);
    EXPECT_TRUE(it2 == it3);
    EXPECT_TRUE(it1 < it2);
    EXPECT_TRUE(it2 > it1);
    EXPECT_TRUE(it2 <= it3);
    EXPECT_TRUE(it2 >= it3);
    EXPECT_TRUE(it1 <= it2);
    EXPECT_TRUE(it2 >= it1);
}

TEST(deque_test, const_iterators) {
    // Arrange
    tempest::deque<int> d;
    for (int i = 0; i < 5; ++i) {
        d.push_back(i);
    }
    const tempest::deque<int>& cd = d;

    // Act & Assert
    int expected = 0;
    for (auto it = cd.begin(); it != cd.end(); ++it) {
        EXPECT_EQ(*it, expected++);
    }

    expected = 0;
    for (auto it = d.cbegin(); it != d.cend(); ++it) {
        EXPECT_EQ(*it, expected++);
    }

    expected = 4;
    for (auto it = cd.rbegin(); it != cd.rend(); ++it) {
        EXPECT_EQ(*it, expected--);
    }

    expected = 4;
    for (auto it = d.crbegin(); it != d.crend(); ++it) {
        EXPECT_EQ(*it, expected--);
    }
}

TEST(deque_test, complex_operations) {
    // Arrange
    tempest::deque<int> d;

    // Act
    for (int i = 0; i < 100; ++i) {
        d.push_back(i);
    }
    for (int i = 1; i <= 100; ++i) {
        d.push_front(-i);
    }

    // Assert
    EXPECT_EQ(d.size(), 200);
    EXPECT_EQ(d.front(), -100);
    EXPECT_EQ(d.back(), 99);

    // Act
    for (int i = 0; i < 50; ++i) {
        d.pop_front();
    }

    // Assert
    EXPECT_EQ(d.size(), 150);
    EXPECT_EQ(d.front(), -50);

    // Act
    for (int i = 0; i < 50; ++i) {
        d.pop_back();
    }

    // Assert
    EXPECT_EQ(d.size(), 100);
    EXPECT_EQ(d.back(), 49);
    EXPECT_EQ(d.front(), -50);
}