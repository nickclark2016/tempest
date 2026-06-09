#include <gtest/gtest.h>

#include <tempest/cstring_view.hpp>
#include <tempest/string_view.hpp>
#include <tempest/string.hpp>

TEST(cstring_view, construct_from_cstring)
{
    auto view = tempest::cstring_view("hello");

    EXPECT_EQ(view.size(), 5);
    EXPECT_EQ(view.length(), 5);
    EXPECT_EQ(view[0], 'h');
    EXPECT_EQ(view[1], 'e');
    EXPECT_EQ(view[2], 'l');
    EXPECT_EQ(view[3], 'l');
    EXPECT_EQ(view[4], 'o');

    EXPECT_EQ(view.front(), 'h');
    EXPECT_EQ(view.back(), 'o');
}

TEST(cstring_view, construct_with_literal)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;

    EXPECT_EQ(view.size(), 5);
    EXPECT_EQ(view.length(), 5);
    EXPECT_EQ(view[0], 'h');
    EXPECT_EQ(view[1], 'e');
    EXPECT_EQ(view[2], 'l');
    EXPECT_EQ(view[3], 'l');
    EXPECT_EQ(view[4], 'o');

    EXPECT_EQ(view.front(), 'h');
    EXPECT_EQ(view.back(), 'o');
}

TEST(cstring_view, construct_from_string)
{
    tempest::string str = "hello";
    auto view = tempest::cstring_view(str);

    EXPECT_EQ(view.size(), 5);
    EXPECT_EQ(view.length(), 5);
    EXPECT_EQ(view[0], 'h');
    EXPECT_EQ(view[1], 'e');
    EXPECT_EQ(view[2], 'l');
    EXPECT_EQ(view[3], 'l');
    EXPECT_EQ(view[4], 'o');

    EXPECT_EQ(view.front(), 'h');
    EXPECT_EQ(view.back(), 'o');
}

TEST(cstring_view, construct_default)
{
    auto view = tempest::cstring_view();

    EXPECT_TRUE(view.empty());
    EXPECT_EQ(view.size(), 0);
    EXPECT_EQ(view.length(), 0);
}

TEST(cstring_view, construct_from_string_view)
{
    tempest::string_view sv1("hello");
    auto sv2 = tempest::cstring_view(sv1);

    EXPECT_EQ(sv2.size(), 5);
    EXPECT_EQ(sv2.length(), 5);
    EXPECT_EQ(sv2[0], 'h');
    EXPECT_EQ(sv2[1], 'e');
    EXPECT_EQ(sv2[2], 'l');
    EXPECT_EQ(sv2[3], 'l');
    EXPECT_EQ(sv2[4], 'o');

    EXPECT_EQ(sv2.front(), 'h');
    EXPECT_EQ(sv2.back(), 'o');
}

TEST(cstring_view, construct_from_pointer_and_size)
{
    const char* str = "hello";
    auto view = tempest::cstring_view(str, 5U); // NOLINT

    EXPECT_EQ(view.size(), 5);
    EXPECT_EQ(view.length(), 5);
    EXPECT_EQ(view[0], 'h');
    EXPECT_EQ(view[1], 'e');
    EXPECT_EQ(view[2], 'l');
    EXPECT_EQ(view[3], 'l');
    EXPECT_EQ(view[4], 'o');

    EXPECT_EQ(view.front(), 'h');
    EXPECT_EQ(view.back(), 'o');
}

TEST(cstring_view, search_sv_cstring)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    const char* target = "ell";

    EXPECT_EQ(tempest::search(source, target), source.begin() + 1);
}

TEST(cstring_view, search_sv_cstring_view)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "ell"_csv;

    EXPECT_EQ(tempest::search(source, target), source.begin() + 1);
}

TEST(cstring_view, search_sv_char)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    char target = 'e';

    EXPECT_EQ(tempest::search(source, target), source.begin() + 1);
}

TEST(cstring_view, search_sv_iterator)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "ell"_csv;

    EXPECT_EQ(tempest::search(source, target.begin(), target.end()), source.begin() + 1);
}

TEST(cstring_view, reverse_search_sv_cstring)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    const char* target = "ell";

    EXPECT_EQ(tempest::reverse_search(source, target), source.begin() + 1);
}

TEST(cstring_view, reverse_search_sv_cstring_view)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "ell"_csv;

    EXPECT_EQ(tempest::reverse_search(source, target), source.begin() + 1);
}

TEST(cstring_view, reverse_search_sv_char)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    char target = 'e';

    EXPECT_EQ(tempest::reverse_search(source, target), source.begin() + 1);
}

TEST(cstring_view, reverse_search_sv_iterator)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "ell"_csv;

    EXPECT_EQ(tempest::reverse_search(source, target.begin(), target.end()), source.begin() + 1);
}

TEST(cstring_view, starts_with_sv_cstring)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    const char* prefix = "he";

    EXPECT_TRUE(tempest::starts_with(view, prefix));
}

TEST(cstring_view, starts_with_sv_cstring_view)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    auto prefix = "he"_csv;

    EXPECT_TRUE(tempest::starts_with(view, prefix));
}

TEST(cstring_view, starts_with_sv_char)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    char prefix = 'h';

    EXPECT_TRUE(tempest::starts_with(view, prefix));
}

TEST(cstring_view, starts_with_sv_iterator)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    auto prefix = "he"_csv;

    EXPECT_TRUE(tempest::starts_with(view, prefix.begin(), prefix.end()));
}

TEST(cstring_view, starts_with_sv_cstring_failure)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    const char* prefix = "hi";

    EXPECT_FALSE(tempest::starts_with(view, prefix));
}

TEST(cstring_view, starts_with_sv_cstring_view_failure)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    auto prefix = "hi"_csv;

    EXPECT_FALSE(tempest::starts_with(view, prefix));
}

TEST(cstring_view, starts_with_sv_char_failure)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    char prefix = 'x';

    EXPECT_FALSE(tempest::starts_with(view, prefix));
}

TEST(cstring_view, starts_with_sv_iterator_failure)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    auto prefix = "hi"_csv;

    EXPECT_FALSE(tempest::starts_with(view, prefix.begin(), prefix.end()));
}

TEST(cstring_view, ends_with_sv_cstring)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    const char* suffix = "lo";

    EXPECT_TRUE(tempest::ends_with(view, suffix));
}

TEST(cstring_view, ends_with_sv_cstring_view)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    auto suffix = "lo"_csv;

    EXPECT_TRUE(tempest::ends_with(view, suffix));
}

TEST(cstring_view, ends_with_sv_char)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    char suffix = 'o';

    EXPECT_TRUE(tempest::ends_with(view, suffix));
}

TEST(cstring_view, ends_with_sv_iterator)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    auto suffix = "lo"_csv;

    EXPECT_TRUE(tempest::ends_with(view, suffix.begin(), suffix.end()));
}

TEST(cstring_view, ends_with_sv_cstring_failure)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    const char* suffix = "xo";

    EXPECT_FALSE(tempest::ends_with(view, suffix));
}

TEST(cstring_view, ends_with_sv_cstring_view_failure)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    auto suffix = "xo"_csv;

    EXPECT_FALSE(tempest::ends_with(view, suffix));
}

TEST(cstring_view, ends_with_sv_char_failure)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    char suffix = 'x';

    EXPECT_FALSE(tempest::ends_with(view, suffix));
}

TEST(cstring_view, ends_with_sv_iterator_failure)
{
    using namespace tempest::literals;

    auto view = "hello"_csv;
    auto suffix = "xo"_csv;

    EXPECT_FALSE(tempest::ends_with(view, suffix.begin(), suffix.end()));
}

TEST(cstring_view, search_first_of_sv)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "el"_csv;

    EXPECT_EQ(tempest::search_first_of(source, target), source.begin() + 1);
}

TEST(cstring_view, search_first_of_sv_iterator)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "el"_csv;

    EXPECT_EQ(tempest::search_first_of(source, target.begin(), target.end()), source.begin() + 1);
}

TEST(cstring_view, search_first_of_sv_char)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    char target = 'e';

    EXPECT_EQ(tempest::search_first_of(source, target), source.begin() + 1);
}

TEST(cstring_view, search_first_of_sv_cstring)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    const char* target = "el";

    EXPECT_EQ(tempest::search_first_of(source, target), source.begin() + 1);
}

TEST(cstring_view, search_first_of_sv_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "xyz"_csv;

    EXPECT_EQ(tempest::search_first_of(source, target), source.end());
}

TEST(cstring_view, search_first_of_sv_iterator_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "xyz"_csv;

    EXPECT_EQ(tempest::search_first_of(source, target.begin(), target.end()), source.end());
}

TEST(cstring_view, search_first_of_sv_char_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    char target = 'x';

    EXPECT_EQ(tempest::search_first_of(source, target), source.end());
}

TEST(cstring_view, search_first_of_sv_cstring_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    const char* target = "xyz";

    EXPECT_EQ(tempest::search_first_of(source, target), source.end());
}

TEST(cstring_view, search_last_of_sv)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "el"_csv;

    EXPECT_EQ(tempest::search_last_of(source, target), source.begin() + 3);
}

TEST(cstring_view, search_last_of_sv_iterator)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "el"_csv;

    EXPECT_EQ(tempest::search_last_of(source, target.begin(), target.end()), source.begin() + 3);
}

TEST(cstring_view, search_last_of_sv_char)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    char target = 'e';

    EXPECT_EQ(tempest::search_last_of(source, target), source.begin() + 1);
}

TEST(cstring_view, search_last_of_sv_cstring)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    const char* target = "el";

    EXPECT_EQ(tempest::search_last_of(source, target), source.begin() + 3);
}

TEST(cstring_view, search_last_of_sv_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "xyz"_csv;

    EXPECT_EQ(tempest::search_last_of(source, target), source.end());
}

TEST(cstring_view, search_last_of_sv_iterator_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "xyz"_csv;

    EXPECT_EQ(tempest::search_last_of(source, target.begin(), target.end()), source.end());
}

TEST(cstring_view, search_last_of_sv_char_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    char target = 'x';

    EXPECT_EQ(tempest::search_last_of(source, target), source.end());
}

TEST(cstring_view, search_last_of_sv_cstring_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    const char* target = "xyz";

    EXPECT_EQ(tempest::search_last_of(source, target), source.end());
}

TEST(cstring_view, search_last_not_of_sv)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "he"_csv;

    EXPECT_EQ(tempest::search_last_not_of(source, target), source.begin() + 4);
}

TEST(cstring_view, search_last_not_of_sv_iterator)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "he"_csv;

    EXPECT_EQ(tempest::search_last_not_of(source, target.begin(), target.end()), source.begin() + 4);
}

TEST(cstring_view, search_last_not_of_sv_char)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    char target = 'h';

    EXPECT_EQ(tempest::search_last_not_of(source, target), source.begin() + 4);
}

TEST(cstring_view, search_last_not_of_sv_cstring)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    const char* target = "he";

    EXPECT_EQ(tempest::search_last_not_of(source, target), source.begin() + 4);
}

TEST(cstring_view, search_last_not_of_sv_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "hello"_csv;

    EXPECT_EQ(tempest::search_last_not_of(source, target), source.end());
}

TEST(cstring_view, search_last_not_of_sv_iterator_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "hello"_csv;

    EXPECT_EQ(tempest::search_last_not_of(source, target.begin(), target.end()), source.end());
}

TEST(cstring_view, search_last_not_of_sv_cstring_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    const char* target = "hello";

    EXPECT_EQ(tempest::search_last_not_of(source, target), source.end());
}

TEST(cstring_view, search_first_not_of_sv)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "he"_csv;

    EXPECT_EQ(tempest::search_first_not_of(source, target), source.begin() + 2);
}

TEST(cstring_view, search_first_not_of_sv_iterator)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "he"_csv;

    EXPECT_EQ(tempest::search_first_not_of(source, target.begin(), target.end()), source.begin() + 2);
}

TEST(cstring_view, search_first_not_of_sv_char)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    char target = 'h';

    EXPECT_EQ(tempest::search_first_not_of(source, target), source.begin() + 1);
}

TEST(cstring_view, search_first_not_of_sv_cstring)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    const char* target = "he";

    EXPECT_EQ(tempest::search_first_not_of(source, target), source.begin() + 2);
}

TEST(cstring_view, search_first_not_of_sv_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "hello"_csv;

    EXPECT_EQ(tempest::search_first_not_of(source, target), source.end());
}

TEST(cstring_view, search_first_not_of_sv_iterator_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    auto target = "hello"_csv;

    EXPECT_EQ(tempest::search_first_not_of(source, target.begin(), target.end()), source.end());
}

TEST(cstring_view, search_first_not_of_sv_cstring_failure)
{
    using namespace tempest::literals;

    auto source = "hello"_csv;
    const char* target = "hello";

    EXPECT_EQ(tempest::search_first_not_of(source, target), source.end());
}

TEST(cstring_view, equality)
{
    using namespace tempest::literals;

    auto view1 = "hello"_csv;
    auto view2 = "hello"_csv;

    EXPECT_TRUE(view1 == view2);
}

TEST(cstring_view, equality_failure)
{
    using namespace tempest::literals;

    auto view1 = "hello"_csv;
    auto view2 = "world"_csv;

    EXPECT_FALSE(view1 == view2);
}

TEST(cstring_view, inequality)
{
    using namespace tempest::literals;

    auto view1 = "hello"_csv;
    auto view2 = "world"_csv;

    EXPECT_TRUE(view1 != view2);
}

TEST(cstring_view, inequality_failure)
{
    using namespace tempest::literals;

    auto view1 = "hello"_csv;
    auto view2 = "hello"_csv;

    EXPECT_FALSE(view1 != view2);
}

TEST(cstring_view, less_than)
{
    using namespace tempest::literals;

    auto view1 = "hello"_csv;
    auto view2 = "world"_csv;

    EXPECT_TRUE(view1 < view2);
}

TEST(cstring_view, less_than_failure)
{
    using namespace tempest::literals;

    auto view1 = "hello"_csv;
    auto view2 = "hello"_csv;

    EXPECT_FALSE(view1 < view2);
}

TEST(cstring_view, less_than_or_equal)
{
    using namespace tempest::literals;

    auto view1 = "hello"_csv;
    auto view2 = "hello"_csv;

    EXPECT_TRUE(view1 <= view2);
}

TEST(cstring_view, less_than_or_equal_failure)
{
    using namespace tempest::literals;

    auto view1 = "hello"_csv;
    auto view2 = "globe"_csv;

    EXPECT_FALSE(view1 <= view2);
}

TEST(cstring_view, greater_than)
{
    using namespace tempest::literals;

    auto view1 = "world"_csv;
    auto view2 = "hello"_csv;

    EXPECT_TRUE(view1 > view2);
}

TEST(cstring_view, greater_than_failure)
{
    using namespace tempest::literals;

    auto view1 = "hello"_csv;
    auto view2 = "hello"_csv;

    EXPECT_FALSE(view1 > view2);
}

TEST(cstring_view, greater_than_or_equal)
{
    using namespace tempest::literals;

    auto view1 = "world"_csv;
    auto view2 = "world"_csv;

    EXPECT_TRUE(view1 >= view2);
}

TEST(cstring_view, greater_than_or_equal_failure)
{
    using namespace tempest::literals;

    auto view1 = "hello"_csv;
    auto view2 = "world"_csv;

    EXPECT_FALSE(view1 >= view2);
}

TEST(cstring_view, compare_less)
{
    using namespace tempest::literals;

    auto view1 = "hello"_csv;
    auto view2 = "world"_csv;

    EXPECT_TRUE(tempest::compare(view1, view2) < 0);
}

TEST(cstring_view, compare_equal)
{
    using namespace tempest::literals;

    auto view1 = "hello"_csv;
    auto view2 = "hello"_csv;

    EXPECT_TRUE(tempest::compare(view1, view2) == 0);
}

TEST(cstring_view, compare_greater)
{
    using namespace tempest::literals;

    auto view1 = "world"_csv;
    auto view2 = "hello"_csv;

    EXPECT_TRUE(tempest::compare(view1, view2) > 0);
}
