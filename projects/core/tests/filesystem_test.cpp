#include <tempest/filesystem.hpp>

#include <gtest/gtest.h>

namespace fs = tempest::filesystem;

TEST(path_detail, convert_wide_to_narrow)
{
    auto narrow_str = fs::detail::convert_wide_to_narrow(L"Hello World");
    EXPECT_EQ(narrow_str, "Hello World");

    narrow_str = fs::detail::convert_wide_to_narrow(L"");
    EXPECT_EQ(narrow_str, "");

    narrow_str = fs::detail::convert_wide_to_narrow(L"Hello\\World");
    EXPECT_EQ(narrow_str, "Hello\\World");

    narrow_str = fs::detail::convert_wide_to_narrow(L"Hello/World");
    EXPECT_EQ(narrow_str, "Hello/World");

    narrow_str =
        fs::detail::convert_wide_to_narrow(L"Some really long path with spaces and special characters !@#$%^&*()_+");
    EXPECT_EQ(narrow_str, "Some really long path with spaces and special characters !@#$%^&*()_+");
}

TEST(path_detail, convert_narrow_to_wide)
{
    auto wide_str = fs::detail::convert_narrow_to_wide("Hello World");
    EXPECT_EQ(wide_str, L"Hello World");

    wide_str = fs::detail::convert_narrow_to_wide("");
    EXPECT_EQ(wide_str, L"");

    wide_str = fs::detail::convert_narrow_to_wide("Hello\\World");
    EXPECT_EQ(wide_str, L"Hello\\World");

    wide_str = fs::detail::convert_narrow_to_wide("Hello/World");
    EXPECT_EQ(wide_str, L"Hello/World");

    wide_str =
        fs::detail::convert_narrow_to_wide("Some really long path with spaces and special characters !@#$%^&*()_+");
    EXPECT_EQ(wide_str, L"Some really long path with spaces and special characters !@#$%^&*()_+");
}

TEST(path, default_constructor)
{
#ifdef _WIN32
    fs::path p;

    EXPECT_EQ(p.native(), L"");
    EXPECT_STREQ(p.c_str(), L"");

    tempest::wstring native_str = p;
    EXPECT_EQ(native_str, L"");
#else
    fs::path p;

    EXPECT_EQ(p.native(), "");
    EXPECT_STREQ(p.c_str(), "");

    tempest::string native_str = p;
    EXPECT_EQ(native_str, "");
}
#endif
}

TEST(path, construct_from_native_path)
{
#ifdef _WIN32
    fs::path p = L"Hello\\World";

    EXPECT_EQ(p.native(), L"Hello\\World");
    EXPECT_STREQ(p.c_str(), L"Hello\\World");

    tempest::wstring native_str = p;
    EXPECT_EQ(native_str, L"Hello\\World");
#else
    fs::path p = "Hello/World";

    EXPECT_EQ(p.native(), "Hello/World");
    EXPECT_STREQ(p.c_str(), "Hello/World");

    tempest::string native_str = p;
    EXPECT_EQ(native_str, "Hello/World");
#endif
}

TEST(path, construct_from_char_path)
{
    fs::path p = "Hello/World";
#ifdef _WIN32
    EXPECT_EQ(p.native(), L"Hello/World");
    EXPECT_STREQ(p.c_str(), L"Hello/World");

    tempest::wstring native_str = p;
    EXPECT_EQ(native_str, L"Hello/World");
#else
    EXPECT_EQ(p.native(), "Hello/World");
    EXPECT_STREQ(p.c_str(), "Hello/World");

    tempest::string native_str = p;
    EXPECT_EQ(native_str, "Hello/World");
#endif
}

TEST(path, construct_from_wchar_path)
{
    fs::path p = L"Hello\\World";
#ifdef _WIN32
    EXPECT_EQ(p.native(), L"Hello\\World");
    EXPECT_STREQ(p.c_str(), L"Hello\\World");

    tempest::wstring native_str = p;
    EXPECT_EQ(native_str, L"Hello\\World");
#else
    EXPECT_EQ(p.native(), "Hello\\World");
    EXPECT_STREQ(p.c_str(), "Hello\\World");

    tempest::string native_str = p;
    EXPECT_EQ(native_str, "Hello\\World");
#endif
}

TEST(path, root_name)
{
    fs::path win_style_path = L"C:\\Users\\User\\Documents\\file.txt";
    fs::path unix_style_path = "/home/user/documents/file.txt";
    fs::path win_style_root_path = L"C:\\";
    fs::path unix_style_root_path = "/";
    fs::path empty_path = "";
    fs::path relative_win_path = L"Documents\\file.txt";
    fs::path relative_unix_path = "documents/file.txt";
    fs::path win_unc_path = L"\\\\server\\share\\file.txt";
    fs::path unix_unc_path = "//server/share/file.txt";

    ASSERT_TRUE(win_style_path.has_root_name());
    ASSERT_FALSE(unix_style_path.has_root_name());
    ASSERT_TRUE(win_style_root_path.has_root_name());
    ASSERT_FALSE(unix_style_root_path.has_root_name());
    ASSERT_FALSE(empty_path.has_root_name());
    ASSERT_FALSE(relative_win_path.has_root_name());
    ASSERT_FALSE(relative_unix_path.has_root_name());
    ASSERT_TRUE(win_unc_path.has_root_name());
    ASSERT_TRUE(unix_unc_path.has_root_name());

#ifdef _WIN32
    EXPECT_EQ(win_style_path.root_name().native(), L"C:");
    EXPECT_EQ(unix_style_path.root_name().native(), L"");
    EXPECT_EQ(win_style_root_path.root_name().native(), L"C:");
    EXPECT_EQ(unix_style_root_path.root_name().native(), L"");
    EXPECT_EQ(empty_path.root_name().native(), L"");
    EXPECT_EQ(relative_win_path.root_name().native(), L"");
    EXPECT_EQ(relative_unix_path.root_name().native(), L"");
    EXPECT_EQ(win_unc_path.root_name().native(), L"\\\\server\\share");
    EXPECT_EQ(unix_unc_path.root_name().native(), L"//server/share");
#else
    EXPECT_EQ(win_style_path.root_name().native(), "C:");
    EXPECT_EQ(unix_style_path.root_name().native(), "");
    EXPECT_EQ(win_style_root_path.root_name().native(), "C:");
    EXPECT_EQ(unix_style_root_path.root_name().native(), "");
    EXPECT_EQ(empty_path.root_name().native(), "");
    EXPECT_EQ(relative_win_path.root_name().native(), "");
    EXPECT_EQ(relative_unix_path.root_name().native(), "");
    EXPECT_EQ(win_unc_path.root_name().native(), "\\\\server\\share");
    EXPECT_EQ(unix_unc_path.root_name().native(), "//server\\share");
#endif
}

TEST(path, root_directory)
{
    fs::path win_style_path = L"C:\\Users\\User\\Documents\\file.txt";
    fs::path unix_style_path = "/home/user/documents/file.txt";
    fs::path win_style_root_path = L"C:\\";
    fs::path unix_style_root_path = "/";
    fs::path empty_path = "";
    fs::path relative_win_path = L"Documents\\file.txt";
    fs::path relative_unix_path = "documents/file.txt";
    fs::path win_unc_path = L"\\\\server\\share\\file.txt";
    fs::path unix_unc_path = "//server/share/file.txt";

    ASSERT_TRUE(win_style_path.has_root_directory());
    ASSERT_TRUE(unix_style_path.has_root_directory());
    ASSERT_TRUE(win_style_root_path.has_root_directory());
    ASSERT_TRUE(unix_style_root_path.has_root_directory());
    ASSERT_FALSE(empty_path.has_root_directory());
    ASSERT_FALSE(relative_win_path.has_root_directory());
    ASSERT_FALSE(relative_unix_path.has_root_directory());
    ASSERT_TRUE(win_unc_path.has_root_directory());
    ASSERT_TRUE(unix_unc_path.has_root_directory());

#ifdef _WIN32
    EXPECT_EQ(win_style_path.root_directory().native(), L"\\");
    EXPECT_EQ(unix_style_path.root_directory().native(), L"/");
    EXPECT_EQ(win_style_root_path.root_directory().native(), L"\\");
    EXPECT_EQ(unix_style_root_path.root_directory().native(), L"/");
    EXPECT_EQ(empty_path.root_directory().native(), L"");
    EXPECT_EQ(relative_win_path.root_directory().native(), L"");
    EXPECT_EQ(relative_unix_path.root_directory().native(), L"");
    EXPECT_EQ(win_unc_path.root_directory().native(), L"\\");
    EXPECT_EQ(unix_unc_path.root_directory().native(), L"/");
#else
    EXPECT_EQ(win_style_path.root_directory().native(), "\\");
    EXPECT_EQ(unix_style_path.root_directory().native(), "/");
    EXPECT_EQ(win_style_root_path.root_directory().native(), "\\");
    EXPECT_EQ(unix_style_root_path.root_directory().native(), "/");
    EXPECT_EQ(empty_path.root_directory().native(), "");
    EXPECT_EQ(relative_win_path.root_directory().native(), "");
    EXPECT_EQ(relative_unix_path.root_directory().native(), "");
    EXPECT_EQ(win_unc_path.root_directory().native(), "\\");
    EXPECT_EQ(unix_unc_path.root_directory().native(), "/");
#endif
}

TEST(path, root_path)
{
    fs::path win_style_path = L"C:\\Users\\User\\Documents\\file.txt";
    fs::path unix_style_path = "/home/user/documents/file.txt";
    fs::path win_style_root_path = L"C:\\";
    fs::path unix_style_root_path = "/";
    fs::path empty_path = "";
    fs::path relative_win_path = L"Documents\\file.txt";
    fs::path relative_unix_path = "documents/file.txt";
    fs::path win_unc_path = L"\\\\server\\share\\file.txt";
    fs::path unix_unc_path = "//server/share/file.txt";

    ASSERT_TRUE(win_style_path.has_root_path());
    ASSERT_TRUE(unix_style_path.has_root_path());
    ASSERT_TRUE(win_style_root_path.has_root_path());
    ASSERT_TRUE(unix_style_root_path.has_root_path());
    ASSERT_FALSE(empty_path.has_root_path());
    ASSERT_FALSE(relative_win_path.has_root_path());
    ASSERT_FALSE(relative_unix_path.has_root_path());
    ASSERT_TRUE(win_unc_path.has_root_path());
    ASSERT_TRUE(unix_unc_path.has_root_path());

#ifdef _WIN32
    EXPECT_EQ(win_style_path.root_path().native(), L"C:\\");
    EXPECT_EQ(unix_style_path.root_path().native(), L"/");
    EXPECT_EQ(win_style_root_path.root_path().native(), L"C:\\");
    EXPECT_EQ(unix_style_root_path.root_path().native(), L"/");
    EXPECT_EQ(empty_path.root_path().native(), L"");
    EXPECT_EQ(relative_win_path.root_path().native(), L"");
    EXPECT_EQ(relative_unix_path.root_path().native(), L"");
    EXPECT_EQ(win_unc_path.root_path().native(), L"\\\\server\\share\\");
    EXPECT_EQ(unix_unc_path.root_path().native(), L"//server/share/");
#else
    EXPECT_EQ(win_style_path.root_path().native(), "C:\\");
    EXPECT_EQ(unix_style_path.root_path().native(), "/");
    EXPECT_EQ(win_style_root_path.root_path().native(), "C:\\");
    EXPECT_EQ(unix_style_root_path.root_path().native(), "/");
    EXPECT_EQ(empty_path.root_path().native(), "");
    EXPECT_EQ(relative_win_path.root_path().native(), "");
    EXPECT_EQ(relative_unix_path.root_path().native(), "");
    EXPECT_EQ(win_unc_path.root_path().native(), "\\\\server\\share\\");
    EXPECT_EQ(unix_unc_path.root_path().native(), "//server/share/");
#endif
}

TEST(path, parent_path)
{
    fs::path win_style_path = L"C:\\Users\\User\\Documents\\file.txt";
    fs::path win_style_dir_path = L"C:\\Users\\User\\Documents\\";
    fs::path unix_style_path = "/home/user/documents/file.txt";
    fs::path unix_style_dir_path = "/home/user/documents/";
    fs::path empty_path = "";
    fs::path relative_win_path = L"Documents\\file.txt";
    fs::path relative_unix_path = "documents/file.txt";
    fs::path win_unc_path = L"\\\\server\\share\\file.txt";
    fs::path unix_unc_path = "//server/share/file.txt";
    fs::path win_root_drive_path = L"C:\\";
    fs::path unix_root_drive_path = "/";
    fs::path win_unc_root_path = L"\\\\server";
    fs::path unix_unc_root_path = "//server";

    ASSERT_TRUE(win_style_path.has_parent_path());
    ASSERT_TRUE(win_style_dir_path.has_parent_path());
    ASSERT_TRUE(unix_style_path.has_parent_path());
    ASSERT_TRUE(unix_style_dir_path.has_parent_path());
    ASSERT_FALSE(empty_path.has_parent_path());
    ASSERT_TRUE(relative_win_path.has_parent_path());
    ASSERT_TRUE(relative_unix_path.has_parent_path());
    ASSERT_TRUE(win_unc_path.has_parent_path());
    ASSERT_TRUE(unix_unc_path.has_parent_path());
    ASSERT_FALSE(win_root_drive_path.has_parent_path());
    ASSERT_FALSE(unix_root_drive_path.has_parent_path());

#ifdef _WIN32
    EXPECT_EQ(win_style_path.parent_path().native(), L"C:\\Users\\User\\Documents");
    EXPECT_EQ(win_style_dir_path.parent_path().native(), L"C:\\Users\\User");
    EXPECT_EQ(unix_style_path.parent_path().native(), L"/home/user/documents");
    EXPECT_EQ(unix_style_dir_path.parent_path().native(), L"/home/user");
    EXPECT_EQ(empty_path.parent_path().native(), L"");
    EXPECT_EQ(relative_win_path.parent_path().native(), L"Documents");
    EXPECT_EQ(relative_unix_path.parent_path().native(), L"documents");
    EXPECT_EQ(win_unc_path.parent_path().native(), L"\\\\server\\share");
    EXPECT_EQ(unix_unc_path.parent_path().native(), L"//server/share");
    EXPECT_EQ(win_root_drive_path.parent_path().native(), L"");
    EXPECT_EQ(unix_root_drive_path.parent_path().native(), L"");
#else
    EXPECT_EQ(win_style_path.parent_path().native(), "C:\\Users\\User\\Documents");
    EXPECT_EQ(win_style_dir_path.parent_path().native(), "C:\\Users\\User");
    EXPECT_EQ(unix_style_path.parent_path().native(), "/home/user/documents");
    EXPECT_EQ(unix_style_dir_path.parent_path().native(), "/home/user");
    EXPECT_EQ(empty_path.parent_path().native(), "");
    EXPECT_EQ(relative_win_path.parent_path().native(), "Documents");
    EXPECT_EQ(relative_unix_path.parent_path().native(), "documents");
    EXPECT_EQ(win_unc_path.parent_path().native(), "\\\\server\\share");
    EXPECT_EQ(unix_unc_path.parent_path().native(), "//server/share");
    EXPECT_EQ(win_root_drive_path.parent_path().native(), "");
    EXPECT_EQ(unix_root_drive_path.parent_path().native(), "");
#endif
}

TEST(path, relative_path)
{
    fs::path win_style_path = L"C:\\Users\\User\\Documents\\file.txt";
    fs::path unix_style_path = "/home/user/documents/file.txt";
    fs::path empty_path = "";
    fs::path relative_win_path = L"Documents\\file.txt";
    fs::path relative_unix_path = "documents/file.txt";
    fs::path win_root_path = L"C:\\";
    fs::path unix_root_path = "/";
    fs::path win_unc_path = L"\\\\server\\share\\file.txt";
    fs::path unix_unc_path = "//server/share/file.txt";
    fs::path win_unc_root_path = L"\\\\server\\share";
    fs::path unix_unc_root_path = "//server/share";
    fs::path unc_root_with_trailing_slash = L"\\\\server\\share\\";

    ASSERT_TRUE(win_style_path.has_relative_path());
    ASSERT_TRUE(unix_style_path.has_relative_path());
    ASSERT_FALSE(empty_path.has_relative_path());
    ASSERT_TRUE(relative_win_path.has_relative_path());
    ASSERT_TRUE(relative_unix_path.has_relative_path());
    ASSERT_FALSE(win_root_path.has_relative_path());
    ASSERT_FALSE(unix_root_path.has_relative_path());
    ASSERT_TRUE(win_unc_path.has_relative_path());
    ASSERT_TRUE(unix_unc_path.has_relative_path());
    ASSERT_FALSE(win_unc_root_path.has_relative_path());
    ASSERT_FALSE(unix_unc_root_path.has_relative_path());
    ASSERT_FALSE(unc_root_with_trailing_slash.has_relative_path());

#ifdef _WIN32
    ASSERT_EQ(win_style_path.relative_path().native(), L"Users\\User\\Documents\\file.txt");
    ASSERT_EQ(unix_style_path.relative_path().native(), L"home/user/documents/file.txt");
    ASSERT_EQ(empty_path.relative_path().native(), L"");
    ASSERT_EQ(relative_win_path.relative_path().native(), L"Documents\\file.txt");
    ASSERT_EQ(relative_unix_path.relative_path().native(), L"documents/file.txt");
    ASSERT_EQ(win_root_path.relative_path().native(), L"");
    ASSERT_EQ(unix_root_path.relative_path().native(), L"");
    ASSERT_EQ(win_unc_path.relative_path().native(), L"file.txt");
    ASSERT_EQ(unix_unc_path.relative_path().native(), L"file.txt");
    ASSERT_EQ(win_unc_root_path.relative_path().native(), L"");
    ASSERT_EQ(unix_unc_root_path.relative_path().native(), L"");
    ASSERT_EQ(unc_root_with_trailing_slash.relative_path().native(), L"");
#else
    ASSERT_EQ(win_style_path.relative_path().native(), "Users/User/Documents/file.txt");
    ASSERT_EQ(unix_style_path.relative_path().native(), "home/user/documents/file.txt");
    ASSERT_EQ(empty_path.relative_path().native(), "");
    ASSERT_EQ(relative_win_path.relative_path().native(), "Documents/file.txt");
    ASSERT_EQ(relative_unix_path.relative_path().native(), "documents/file.txt");
    ASSERT_EQ(win_root_path.relative_path().native(), "");
    ASSERT_EQ(unix_root_path.relative_path().native(), "");
    ASSERT_EQ(win_unc_path.relative_path().native(), "file.txt");
    ASSERT_EQ(unix_unc_path.relative_path().native(), "file.txt");
    ASSERT_EQ(win_unc_root_path.relative_path().native(), "");
    ASSERT_EQ(unix_unc_root_path.relative_path().native(), "");
    ASSERT_EQ(unc_root_with_trailing_slash.relative_path().native(), "");
#endif
}

TEST(path, filename)
{
    fs::path win_style_path = L"C:\\Users\\User\\Documents\\file.txt";
    fs::path unix_style_path = "/home/user/documents/file.txt";
    fs::path empty_path = "";
    fs::path relative_win_path = L"Documents\\file.txt";
    fs::path relative_unix_path = "documents/file.txt";
    fs::path win_unc_path = L"\\\\server\\share\\file.txt";
    fs::path unix_unc_path = "//server/share/file.txt";
    fs::path win_root_drive_path = L"C:\\";
    fs::path unix_root_drive_path = "/";
    fs::path win_unc_root_path = L"\\\\server\\share";
    fs::path unix_unc_root_path = "//server/share";
    fs::path win_unc_root_with_trailing_slash = L"\\\\server\\share\\";
    fs::path unix_unc_root_with_trailing_slash = "//server/share/";

    ASSERT_TRUE(win_style_path.has_filename());
    ASSERT_TRUE(unix_style_path.has_filename());
    ASSERT_FALSE(empty_path.has_filename());
    ASSERT_TRUE(relative_win_path.has_filename());
    ASSERT_TRUE(relative_unix_path.has_filename());
    ASSERT_TRUE(win_unc_path.has_filename());
    ASSERT_TRUE(unix_unc_path.has_filename());
    ASSERT_FALSE(win_root_drive_path.has_filename());
    ASSERT_FALSE(unix_root_drive_path.has_filename());
    ASSERT_FALSE(win_unc_root_path.has_filename());
    ASSERT_FALSE(unix_unc_root_path.has_filename());
    ASSERT_FALSE(win_unc_root_with_trailing_slash.has_filename());
    ASSERT_FALSE(unix_unc_root_with_trailing_slash.has_filename());

#ifdef _WIN32
    EXPECT_EQ(win_style_path.filename().native(), L"file.txt");
    EXPECT_EQ(unix_style_path.filename().native(), L"file.txt");
    EXPECT_EQ(empty_path.filename().native(), L"");
    EXPECT_EQ(relative_win_path.filename().native(), L"file.txt");
    EXPECT_EQ(relative_unix_path.filename().native(), L"file.txt");
    EXPECT_EQ(win_unc_path.filename().native(), L"file.txt");
    EXPECT_EQ(unix_unc_path.filename().native(), L"file.txt");
    EXPECT_EQ(win_root_drive_path.filename().native(), L"");
    EXPECT_EQ(unix_root_drive_path.filename().native(), L"");
    EXPECT_EQ(win_unc_root_path.filename().native(), L"");
    EXPECT_EQ(unix_unc_root_path.filename().native(), L"");
    EXPECT_EQ(win_unc_root_with_trailing_slash.filename().native(), L"");
    EXPECT_EQ(unix_unc_root_with_trailing_slash.filename().native(), L"");
#else
    EXPECT_EQ(win_style_path.filename().native(), "file.txt");
    EXPECT_EQ(unix_style_path.filename().native(), "file.txt");
    EXPECT_EQ(empty_path.filename().native(), "");
    EXPECT_EQ(relative_win_path.filename().native(), "file.txt");
    EXPECT_EQ(relative_unix_path.filename().native(), "file.txt");
    EXPECT_EQ(win_unc_path.filename().native(), "file.txt");
    EXPECT_EQ(unix_unc_path.filename().native(), "file.txt");
    EXPECT_EQ(win_root_drive_path.filename().native(), "");
    EXPECT_EQ(unix_root_drive_path.filename().native(), "");
    EXPECT_EQ(win_unc_root_path.filename().native(), "");
    EXPECT_EQ(unix_unc_root_path.filename().native(), "");
    EXPECT_EQ(win_unc_root_with_trailing_slash.filename().native(), "");
    EXPECT_EQ(unix_unc_root_with_trailing_slash.filename().native(), "");
#endif
}

TEST(path, stem)
{
    fs::path win_style_path = L"C:\\Users\\User\\Documents\\file.txt";
    fs::path unix_style_path = "/home/user/documents/file.txt";
    fs::path empty_path = "";
    fs::path relative_win_path = L"Documents\\file.txt";
    fs::path relative_unix_path = "documents/file.txt";
    fs::path just_filename = "file.txt";
    fs::path start_with_dot = ".file";
    fs::path start_with_dot_and_extension = ".file.txt";

    ASSERT_TRUE(win_style_path.has_stem());
    ASSERT_TRUE(unix_style_path.has_stem());
    ASSERT_FALSE(empty_path.has_stem());
    ASSERT_TRUE(relative_win_path.has_stem());
    ASSERT_TRUE(relative_unix_path.has_stem());
    ASSERT_TRUE(just_filename.has_stem());
    ASSERT_TRUE(start_with_dot.has_stem());
    ASSERT_TRUE(start_with_dot_and_extension.has_stem());

#ifdef _WIN32
    EXPECT_EQ(win_style_path.stem().native(), L"file");
    EXPECT_EQ(unix_style_path.stem().native(), L"file");
    EXPECT_EQ(empty_path.stem().native(), L"");
    EXPECT_EQ(relative_win_path.stem().native(), L"file");
    EXPECT_EQ(relative_unix_path.stem().native(), L"file");
    EXPECT_EQ(just_filename.stem().native(), L"file");
    EXPECT_EQ(start_with_dot.stem().native(), L".file");
    EXPECT_EQ(start_with_dot_and_extension.stem().native(), L".file");
#else
    EXPECT_EQ(win_style_path.stem().native(), "file");
    EXPECT_EQ(unix_style_path.stem().native(), "file");
    EXPECT_EQ(empty_path.stem().native(), "");
    EXPECT_EQ(relative_win_path.stem().native(), "file");
    EXPECT_EQ(relative_unix_path.stem().native(), "file");
    EXPECT_EQ(just_filename.stem().native(), "file");
    EXPECT_EQ(start_with_dot.stem().native(), ".file");
    EXPECT_EQ(start_with_dot_and_extension.stem().native(), ".file");
#endif
}

TEST(path, extension)
{
    fs::path win_style_path = L"C:\\Users\\User\\Documents\\file.txt";
    fs::path unix_style_path = "/home/user/documents/file.txt";
    fs::path empty_path = "";
    fs::path relative_win_path = L"Documents\\file.txt";
    fs::path relative_unix_path = "documents/file.txt";
    fs::path just_filename = "file.txt";
    fs::path no_extension = "file";
    fs::path start_with_dot = ".file";
    fs::path start_with_dot_and_extension = ".file.txt";

    ASSERT_TRUE(win_style_path.has_extension());
    ASSERT_TRUE(unix_style_path.has_extension());
    ASSERT_FALSE(empty_path.has_extension());
    ASSERT_TRUE(relative_win_path.has_extension());
    ASSERT_TRUE(relative_unix_path.has_extension());
    ASSERT_TRUE(just_filename.has_extension());
    ASSERT_FALSE(no_extension.has_extension());
    ASSERT_FALSE(start_with_dot.has_extension());
    ASSERT_TRUE(start_with_dot_and_extension.has_extension());

#ifdef _WIN32
    EXPECT_EQ(win_style_path.extension().native(), L".txt");
    EXPECT_EQ(unix_style_path.extension().native(), L".txt");
    EXPECT_EQ(empty_path.extension().native(), L"");
    EXPECT_EQ(relative_win_path.extension().native(), L".txt");
    EXPECT_EQ(relative_unix_path.extension().native(), L".txt");
    EXPECT_EQ(just_filename.extension().native(), L".txt");
    EXPECT_EQ(no_extension.extension().native(), L"");
    EXPECT_EQ(start_with_dot.extension().native(), L"");
    EXPECT_EQ(start_with_dot_and_extension.extension().native(), L".txt");
#else
    EXPECT_EQ(win_style_path.extension().native(), ".txt");
    EXPECT_EQ(unix_style_path.extension().native(), ".txt");
    EXPECT_EQ(empty_path.extension().native(), "");
    EXPECT_EQ(relative_win_path.extension().native(), ".txt");
    EXPECT_EQ(relative_unix_path.extension().native(), ".txt");
    EXPECT_EQ(just_filename.extension().native(), ".txt");
    EXPECT_EQ(no_extension.extension().native(), "");
    EXPECT_EQ(start_with_dot.extension().native(), "");
    EXPECT_EQ(start_with_dot_and_extension.extension().native(), ".txt");
#endif
}

TEST(path, append)
{
    fs::path no_roots = fs::path("hello").append("world");
    fs::path win_style_left_root = fs::path("C:\\hello").append("world");
    fs::path win_style_right_root = fs::path("hello").append("C:\\world");
    fs::path win_style_both_roots = fs::path("C:\\hello").append("C:\\world");
    fs::path win_style_unc_left = fs::path("\\\\server\\share").append("file.txt");
    fs::path win_style_unc_right = fs::path("file.txt").append("\\\\server\\share");
    fs::path unix_style_left_root = fs::path("/hello").append("world");
    fs::path unix_style_right_root = fs::path("hello").append("/world");
    fs::path unix_style_both_roots = fs::path("/hello").append("/world");

#ifdef _WIN32
    EXPECT_EQ(no_roots.native(), L"hello\\world");
    EXPECT_EQ(win_style_left_root.native(), L"C:\\hello");
    EXPECT_EQ(win_style_right_root.native(), L"C:\\world");
    EXPECT_EQ(win_style_both_roots.native(), L"C:\\world");
    EXPECT_EQ(win_style_unc_left.native(), L"\\\\server\\share\\file.txt");
    EXPECT_EQ(win_style_unc_right.native(), L"\\\\server\\share");
    EXPECT_EQ(unix_style_left_root.native(), L"/hello/world");
    EXPECT_EQ(unix_style_right_root.native(), L"/world");
    EXPECT_EQ(unix_style_both_roots.native(), L"/world");
#else
    EXPECT_EQ(no_roots.native(), "hello/world");
    EXPECT_EQ(win_style_left_root.native(), "C:\\hello");
    EXPECT_EQ(win_style_right_root.native(), "C:\\world");
    EXPECT_EQ(win_style_both_roots.native(), "C:\\world");
    EXPECT_EQ(win_style_unc_left.native(), "\\\\server\\share\\file.txt");
    EXPECT_EQ(win_style_unc_right.native(), "\\\\server\\share");
    EXPECT_EQ(unix_style_left_root.native(), "/world");
    EXPECT_EQ(unix_style_right_root.native(), "/world");
#endif
}

TEST(path, concat)
{
    fs::path no_roots = fs::path("hello").concat("world");
    fs::path concat_with_slash = fs::path("hello").concat("/world");

#ifdef _WIN32
    EXPECT_EQ(no_roots.native(), L"helloworld");
    EXPECT_EQ(concat_with_slash.native(), L"hello/world");
#else
    EXPECT_EQ(no_roots.native(), "helloworld");
    EXPECT_EQ(concat_with_slash.native(), "hello/world");
#endif
}

TEST(path, divide_operator)
{
    fs::path no_roots = fs::path("hello") / "world";
    fs::path win_style_left_root = fs::path("C:\\hello") / "world";
    fs::path win_style_right_root = fs::path("hello") / "C:\\world";
    fs::path win_style_both_roots = fs::path("C:\\hello") / "C:\\world";
    fs::path unix_style_left_root = fs::path("/hello") / "world";
    fs::path unix_style_right_root = fs::path("hello") / "/world";

#ifdef _WIN32
    EXPECT_EQ(no_roots.native(), L"hello\\world");
    EXPECT_EQ(win_style_left_root.native(), L"C:\\hello\\world");
    EXPECT_EQ(win_style_right_root.native(), L"C:\\world");
    EXPECT_EQ(win_style_both_roots.native(), L"C:\\world");
    EXPECT_EQ(unix_style_left_root.native(), L"/hello/world");
    EXPECT_EQ(unix_style_right_root.native(), L"/world");
#else
    EXPECT_EQ(no_roots.native(), "hello/world");
    EXPECT_EQ(win_style_left_root.native(), "C:\\hello\\world");
    EXPECT_EQ(win_style_right_root.native(), "C:\\world");
    EXPECT_EQ(win_style_both_roots.native(), "C:\\world");
    EXPECT_EQ(unix_style_left_root.native(), "/hello/world");
    EXPECT_EQ(unix_style_right_root.native(), "/world");
#endif
}