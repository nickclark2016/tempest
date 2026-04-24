#include <tempest/string_view.hpp>

#include <tempest/string.hpp>

#include <gtest/gtest.h>

TEST(string_view, construct_from_cstring)
{
    tempest::string_view sv("hello");

    EXPECT_EQ(sv.length(), 5);
    EXPECT_EQ(sv.size(), 5);
    
    EXPECT_EQ(sv[0], 'h');
    EXPECT_EQ(sv[1], 'e');
    EXPECT_EQ(sv[2], 'l');
    EXPECT_EQ(sv[3], 'l');
    EXPECT_EQ(sv[4], 'o');

    EXPECT_EQ(sv.front(), 'h');
    EXPECT_EQ(sv.back(), 'o');
}

TEST(string_view, construct_with_literal)
{
    using namespace tempest::literals;

    auto sv = "hello"_sv;

    EXPECT_EQ(sv.length(), 5);
    EXPECT_EQ(sv.size(), 5);
    
    EXPECT_EQ(sv[0], 'h');
    EXPECT_EQ(sv[1], 'e');
    EXPECT_EQ(sv[2], 'l');
    EXPECT_EQ(sv[3], 'l');
    EXPECT_EQ(sv[4], 'o');

    EXPECT_EQ(sv.front(), 'h');
    EXPECT_EQ(sv.back(), 'o');
}

TEST(string_view, construct_from_string)
{
    std::string s = "hello";
    tempest::string_view sv(s);

    EXPECT_EQ(sv.length(), 5);
    EXPECT_EQ(sv.size(), 5);
    
    EXPECT_EQ(sv[0], 'h');
    EXPECT_EQ(sv[1], 'e');
    EXPECT_EQ(sv[2], 'l');
    EXPECT_EQ(sv[3], 'l');
    EXPECT_EQ(sv[4], 'o');

    EXPECT_EQ(sv.front(), 'h');
    EXPECT_EQ(sv.back(), 'o');
}

TEST(string_view, construct_from_string_view)
{
    tempest::string_view sv1("hello");
    tempest::string_view sv2(sv1);

    EXPECT_EQ(sv2.length(), 5);
    EXPECT_EQ(sv2.size(), 5);
    
    EXPECT_EQ(sv2[0], 'h');
    EXPECT_EQ(sv2[1], 'e');
    EXPECT_EQ(sv2[2], 'l');
    EXPECT_EQ(sv2[3], 'l');
    EXPECT_EQ(sv2[4], 'o');

    EXPECT_EQ(sv2.front(), 'h');
    EXPECT_EQ(sv2.back(), 'o');
}

TEST(string_view, construct_from_iterators)
{
    std::string s = "hello";
    tempest::string_view sv(s.begin(), s.end());

    EXPECT_EQ(sv.length(), 5);
    EXPECT_EQ(sv.size(), 5);
    
    EXPECT_EQ(sv[0], 'h');
    EXPECT_EQ(sv[1], 'e');
    EXPECT_EQ(sv[2], 'l');
    EXPECT_EQ(sv[3], 'l');
    EXPECT_EQ(sv[4], 'o');

    EXPECT_EQ(sv.front(), 'h');
    EXPECT_EQ(sv.back(), 'o');
}

TEST(string_view, construct_from_pointer_and_size)
{
    const char* s = "hello";
    tempest::string_view sv(s, 5);

    EXPECT_EQ(sv.length(), 5);
    EXPECT_EQ(sv.size(), 5);
    
    EXPECT_EQ(sv[0], 'h');
    EXPECT_EQ(sv[1], 'e');
    EXPECT_EQ(sv[2], 'l');
    EXPECT_EQ(sv[3], 'l');
    EXPECT_EQ(sv[4], 'o');

    EXPECT_EQ(sv.front(), 'h');
    EXPECT_EQ(sv.back(), 'o');
}

TEST(string_view, construct_from_pointer)
{
    const char* s = "hello";
    tempest::string_view sv(s);

    EXPECT_EQ(sv.length(), 5);
    EXPECT_EQ(sv.size(), 5);
    
    EXPECT_EQ(sv[0], 'h');
    EXPECT_EQ(sv[1], 'e');
    EXPECT_EQ(sv[2], 'l');
    EXPECT_EQ(sv[3], 'l');
    EXPECT_EQ(sv[4], 'o');

    EXPECT_EQ(sv.front(), 'h');
    EXPECT_EQ(sv.back(), 'o');
}

TEST(string_view, empty)
{
    tempest::string_view sv;

    EXPECT_TRUE(sv.empty());
}

TEST(string_view, not_empty)
{
    tempest::string_view sv("hello");

    EXPECT_FALSE(sv.empty());
}

TEST(string_view, search_sv)
{
    tempest::string_view s("hello");
    tempest::string_view t("ell");

    EXPECT_EQ(tempest::search(s, t), s.begin() + 1);
}

TEST(string_view, search_sv_iterator)
{
    tempest::string_view s("hello");
    tempest::string_view t("ell");

    EXPECT_EQ(tempest::search(s, t.begin(), t.end()), s.begin() + 1);
}

TEST(string_view, search_sv_char)
{
    tempest::string_view s("hello");
    char t = 'e';

    EXPECT_EQ(tempest::search(s, t), s.begin() + 1);
}

TEST(string_view, search_sv_cstring)
{
    tempest::string_view s("hello");
    const char* t = "ell";

    EXPECT_EQ(tempest::search(s, t), s.begin() + 1);
}

TEST(string_view, reverse_search_sv)
{
    tempest::string_view s("hello");
    tempest::string_view t("ell");

    EXPECT_EQ(tempest::reverse_search(s, t), s.begin() + 1);
}

TEST(string_view, reverse_search_sv_iterator)
{
    tempest::string_view s("hello");
    tempest::string_view t("ell");

    EXPECT_EQ(tempest::reverse_search(s, t.begin(), t.end()), s.begin() + 1);
}

TEST(string_view, reverse_search_sv_char)
{
    tempest::string_view s("hello");
    char t = 'e';

    EXPECT_EQ(tempest::reverse_search(s, t), s.begin() + 1);
}

TEST(string_view, reverse_search_sv_cstring)
{
    tempest::string_view s("hello");
    const char* t = "ell";

    EXPECT_EQ(tempest::reverse_search(s, t), s.begin() + 1);
}

TEST(string_view, starts_with_sv)
{
    tempest::string_view s("hello");
    tempest::string_view t("he");

    EXPECT_TRUE(tempest::starts_with(s, t));
}

TEST(string_view, starts_with_sv_iterator)
{
    tempest::string_view s("hello");
    tempest::string_view t("he");

    EXPECT_TRUE(tempest::starts_with(s, t.begin(), t.end()));
}

TEST(string_view, starts_with_sv_char)
{
    tempest::string_view s("hello");
    char t = 'h';

    EXPECT_TRUE(tempest::starts_with(s, t));
}

TEST(string_view, starts_with_sv_cstring)
{
    tempest::string_view s("hello");
    const char* t = "he";

    EXPECT_TRUE(tempest::starts_with(s, t));
}

TEST(string_view, ends_with_sv)
{
    tempest::string_view s("hello");
    tempest::string_view t("lo");

    EXPECT_TRUE(tempest::ends_with(s, t));
}

TEST(string_view, ends_with_sv_iterator)
{
    tempest::string_view s("hello");
    tempest::string_view t("lo");

    EXPECT_TRUE(tempest::ends_with(s, t.begin(), t.end()));
}

TEST(string_view, ends_with_sv_char)
{
    tempest::string_view s("hello");
    char t = 'o';

    EXPECT_TRUE(tempest::ends_with(s, t));
}

TEST(string_view, ends_with_sv_cstring)
{
    tempest::string_view s("hello");
    const char* t = "lo";

    EXPECT_TRUE(tempest::ends_with(s, t));
}

TEST(string_view, search_first_of_sv)
{
    tempest::string_view s("hello");
    tempest::string_view t("el");

    EXPECT_EQ(tempest::search_first_of(s, t), s.begin() + 1);
}

TEST(string_view, search_first_of_sv_iterator)
{
    tempest::string_view s("hello");
    tempest::string_view t("el");

    EXPECT_EQ(tempest::search_first_of(s, t.begin(), t.end()), s.begin() + 1);
}

TEST(string_view, search_first_of_sv_char)
{
    tempest::string_view s("hello");
    char t = 'e';

    EXPECT_EQ(tempest::search_first_of(s, t), s.begin() + 1);
}

TEST(string_view, search_first_of_sv_cstring)
{
    tempest::string_view s("hello");
    const char* t = "el";

    EXPECT_EQ(tempest::search_first_of(s, t), s.begin() + 1);
}

TEST(string_view, search_last_of_sv)
{
    tempest::string_view s("hello");
    tempest::string_view t("el");

    EXPECT_EQ(tempest::search_last_of(s, t), s.begin() + 3);
}

TEST(string_view, search_last_of_sv_iterator)
{
    tempest::string_view s("hello");
    tempest::string_view t("el");

    EXPECT_EQ(tempest::search_last_of(s, t.begin(), t.end()), s.begin() + 3);
}

TEST(string_view, search_last_of_sv_char)
{
    tempest::string_view s("hello");
    char t = 'e';

    EXPECT_EQ(tempest::search_last_of(s, t), s.begin() + 1);
}

TEST(string_view, search_last_of_sv_cstring)
{
    tempest::string_view s("hello");
    const char* t = "el";

    EXPECT_EQ(tempest::search_last_of(s, t), s.begin() + 3);
}

TEST(string_view, search_first_not_of_sv)
{
    tempest::string_view s("hello");
    tempest::string_view t("he");

    EXPECT_EQ(tempest::search_first_not_of(s, t), s.begin() + 2);
}

TEST(string_view, search_first_not_of_sv_iterator)
{
    tempest::string_view s("hello");
    tempest::string_view t("he");

    EXPECT_EQ(tempest::search_first_not_of(s, t.begin(), t.end()), s.begin() + 2);
}

TEST(string_view, search_first_not_of_sv_char)
{
    tempest::string_view s("hello");
    char t = 'h';

    EXPECT_EQ(tempest::search_first_not_of(s, t), s.begin() + 1);
}

TEST(string_view, search_first_not_of_sv_cstring)
{
    tempest::string_view s("hello");
    const char* t = "he";

    EXPECT_EQ(tempest::search_first_not_of(s, t), s.begin() + 2);
}

TEST(string_view, search_last_not_of_sv)
{
    tempest::string_view s("hello");
    tempest::string_view t("he");

    EXPECT_EQ(tempest::search_last_not_of(s, t), s.begin() + 4);
}

TEST(string_view, search_last_not_of_sv_iterator)
{
    tempest::string_view s("hello");
    tempest::string_view t("he");

    EXPECT_EQ(tempest::search_last_not_of(s, t.begin(), t.end()), s.begin() + 4);
}

TEST(string_view, search_last_not_of_sv_char)
{
    tempest::string_view s("hello");
    char t = 'h';

    EXPECT_EQ(tempest::search_last_not_of(s, t), s.begin() + 4);
}

TEST(string_view, search_last_not_of_sv_cstring)
{
    tempest::string_view s("hello");
    const char* t = "he";

    EXPECT_EQ(tempest::search_last_not_of(s, t), s.begin() + 4);
}

TEST(string_view, equality)
{
    tempest::string_view s("hello");
    tempest::string_view t("hello");

    EXPECT_TRUE(s == t);
}

TEST(string_view, inequality)
{
    tempest::string_view s("hello");
    tempest::string_view t("world");

    EXPECT_TRUE(s != t);
}

TEST(string_view, less_than)
{
    tempest::string_view s("hello");
    tempest::string_view t("world");

    EXPECT_TRUE(s < t);
}

TEST(string_view, less_than_or_equal)
{
    tempest::string_view s("hello");
    tempest::string_view t("hello");

    EXPECT_TRUE(s <= t);
}

TEST(string_view, greater_than)
{
    tempest::string_view s("world");
    tempest::string_view t("hello");

    EXPECT_TRUE(s > t);
}

TEST(string_view, greater_than_or_equal)
{
    tempest::string_view s("world");
    tempest::string_view t("world");

    EXPECT_TRUE(s >= t);
}

TEST(string_view, compare)
{
    tempest::string_view s("hello");
    tempest::string_view t("world");

    EXPECT_TRUE(tempest::compare(s, t) < 0);
}
