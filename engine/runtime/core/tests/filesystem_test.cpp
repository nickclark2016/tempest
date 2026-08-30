#include <tempest/filesystem.hpp>

#include <gtest/gtest.h>

namespace fs = tempest::filesystem;

TEST(path_detail, convert_wide_to_narrow)
{
    auto narrow_str = fs::detail::convert_wide_to_narrow(L"Hello World");
    EXPECT_EQ(narrow_str, "Hello World");
}

TEST(path_detail, convert_narrow_to_wide)
{
    auto wide_str = fs::detail::convert_narrow_to_wide("Hello World");
    EXPECT_EQ(wide_str, L"Hello World");
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
    EXPECT_EQ(unix_unc_path.root_name().native(), "//server/share");
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
    ASSERT_EQ(win_style_path.relative_path().native(), "Users\\User\\Documents\\file.txt");
    ASSERT_EQ(unix_style_path.relative_path().native(), "home/user/documents/file.txt");
    ASSERT_EQ(empty_path.relative_path().native(), "");
    ASSERT_EQ(relative_win_path.relative_path().native(), "Documents\\file.txt");
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
    EXPECT_EQ(unix_style_left_root.native(), "/hello/world");
    EXPECT_EQ(unix_style_right_root.native(), "/world");
    EXPECT_EQ(unix_style_both_roots.native(), "/world");
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

TEST(path, clear)
{
    fs::path p = "Hello/World";
    EXPECT_FALSE(p.empty());

    p.clear();
    EXPECT_TRUE(p.empty());
}

TEST(path, make_preferred)
{
    fs::path no_dirs = fs::path("HelloWorld");
    fs::path mixed_dirs = fs::path("Hello/World\\Test");
    fs::path unix_only_dirs = fs::path("Hello/World/Test");
    fs::path win_only_dirs = fs::path("Hello\\World\\Test");

    no_dirs.make_preferred();
    mixed_dirs.make_preferred();
    unix_only_dirs.make_preferred();
    win_only_dirs.make_preferred();

#ifdef _WIN32
    EXPECT_EQ(no_dirs.native(), L"HelloWorld");
    EXPECT_EQ(mixed_dirs.native(), L"Hello\\World\\Test");
    EXPECT_EQ(unix_only_dirs.native(), L"Hello\\World\\Test");
    EXPECT_EQ(win_only_dirs.native(), L"Hello\\World\\Test");
#else
    EXPECT_EQ(no_dirs.native(), "HelloWorld");
    EXPECT_EQ(mixed_dirs.native(), "Hello/World/Test");
    EXPECT_EQ(unix_only_dirs.native(), "Hello/World/Test");
    EXPECT_EQ(win_only_dirs.native(), "Hello/World/Test");
#endif
}

TEST(path, remove_filename)
{
    fs::path only_filename = fs::path("HelloWorld");
    fs::path win_root = fs::path("C:\\");
    fs::path unix_root = fs::path("/");
    fs::path win_style_path = fs::path("C:\\Users\\User\\Documents\\file.txt");
    fs::path unix_style_path = fs::path("/home/user/documents/file.txt");

    only_filename.remove_filename();
    win_style_path.remove_filename();
    unix_style_path.remove_filename();

#ifdef _WIN32
    EXPECT_EQ(only_filename.native(), L"");
    EXPECT_EQ(win_root.native(), L"C:\\");
    EXPECT_EQ(unix_root.native(), L"/");
    EXPECT_EQ(win_style_path.native(), L"C:\\Users\\User\\Documents");
    EXPECT_EQ(unix_style_path.native(), L"/home/user/documents");
#else
    EXPECT_EQ(only_filename.native(), "");
    EXPECT_EQ(win_root.native(), "C:\\");
    EXPECT_EQ(unix_root.native(), "/");
    EXPECT_EQ(win_style_path.native(), "C:\\Users\\User\\Documents");
    EXPECT_EQ(unix_style_path.native(), "/home/user/documents");
#endif
}

TEST(path, replace_filename)
{
    fs::path only_filename = fs::path("HelloWorld");
    fs::path with_rel_path = fs::path("Documents/file.txt");

    fs::path replacement = "Foo.png";

    only_filename.replace_filename(replacement);
    with_rel_path.replace_filename(replacement);

#ifdef _WIN32
    EXPECT_EQ(only_filename.native(), L"Foo.png");
    EXPECT_EQ(with_rel_path.native(), L"Documents/Foo.png");
#else
    EXPECT_EQ(only_filename.native(), "Foo.png");
    EXPECT_EQ(with_rel_path.native(), "Documents/Foo.png");
#endif
}

// ============================================================================
// directory_entry Tests
// ============================================================================

/// @brief Tests default initialization of directory_entry.
/// Validates that an uninitialized entry holds an empty path, unknown status,
/// invalid size (-1), and reports exists() == false.
TEST(directory_entry, default_constructor)
{
    // 1. Setup & Act
    auto entry = fs::directory_entry{};

    // 2. Assert
    EXPECT_TRUE(entry.path().empty());
    EXPECT_FALSE(entry.exists());
    EXPECT_FALSE(entry.is_directory());
    EXPECT_FALSE(entry.is_regular_file());
    EXPECT_FALSE(entry.is_symlink());
    EXPECT_FALSE(entry.is_block_file());
    EXPECT_FALSE(entry.is_character_file());
    EXPECT_FALSE(entry.is_fifo());
    EXPECT_FALSE(entry.is_socket());
    EXPECT_FALSE(entry.is_other());
    EXPECT_EQ(entry.status().type(), fs::file_type::none);
    EXPECT_EQ(entry.symlink_status().type(), fs::file_type::none);
    EXPECT_EQ(entry.file_size(), static_cast<size_t>(-1));
}

/// @brief Tests constructing a directory_entry from an existing directory path.
/// Validates eager caching of directory attributes and invalid file size.
TEST(directory_entry, construct_from_existing_directory)
{
    // 1. Setup
    auto cwd = fs::current_path();

    // 2. Act
    auto entry = fs::directory_entry{cwd};

    // 3. Assert
    EXPECT_EQ(entry.path(), cwd);
    EXPECT_TRUE(entry.exists());
    EXPECT_TRUE(entry.is_directory());
    EXPECT_FALSE(entry.is_regular_file());
    EXPECT_FALSE(entry.is_symlink());
    EXPECT_EQ(entry.status().type(), fs::file_type::directory);
    EXPECT_EQ(entry.file_size(), static_cast<size_t>(-1));
}

/// @brief Tests constructing a directory_entry from a non-existent path.
/// Validates that exists() is false, status is not_found, and size is -1.
TEST(directory_entry, construct_from_non_existent_path)
{
    // 1. Setup
    auto non_existent = fs::path{"__tempest_non_existent_file_123456.tmp"};

    // 2. Act
    auto entry = fs::directory_entry{non_existent};

    // 3. Assert
    EXPECT_EQ(entry.path(), non_existent);
    EXPECT_FALSE(entry.exists());
    EXPECT_FALSE(entry.is_directory());
    EXPECT_FALSE(entry.is_regular_file());
    EXPECT_EQ(entry.status().type(), fs::file_type::not_found);
    EXPECT_EQ(entry.file_size(), static_cast<size_t>(-1));
}

/// @brief Tests explicit 4-parameter constructor with pre-cached attributes and size.
/// Validates that observer methods return the cached fields directly with zero syscalls.
TEST(directory_entry, construct_with_cached_attributes)
{
    // 1. Setup
    auto synthetic_path = fs::path{"/virtual/test/file.bin"};
    auto synthetic_status =
        fs::file_status{fs::file_type::regular, fs::permissions::owner_read | fs::permissions::owner_write};
    auto synthetic_sym_status =
        fs::file_status{fs::file_type::regular, fs::permissions::owner_read | fs::permissions::owner_write};
    auto synthetic_size = size_t{1048576};

    // 2. Act
    auto entry = fs::directory_entry{synthetic_path, synthetic_status, synthetic_sym_status, synthetic_size};

    // 3. Assert
    EXPECT_EQ(entry.path(), synthetic_path);
    EXPECT_TRUE(entry.exists());
    EXPECT_TRUE(entry.is_regular_file());
    EXPECT_FALSE(entry.is_directory());
    EXPECT_FALSE(entry.is_symlink());
    EXPECT_EQ(entry.status().type(), fs::file_type::regular);
    EXPECT_EQ(entry.status().perms(), fs::permissions::owner_read | fs::permissions::owner_write);
    EXPECT_EQ(entry.symlink_status().type(), fs::file_type::regular);
    EXPECT_EQ(entry.file_size(), synthetic_size);
}

// ============================================================================
// directory_iterator Tests
// ============================================================================

/// @brief Tests directory traversal over current working directory.
/// Validates range-for iteration, non-empty enumeration, and absence of '.' / '..'.
TEST(directory_iterator, traverse_current_directory)
{
    // 1. Setup
    auto cwd = fs::current_path();
    auto count = size_t{0};

    // 2. Act
    for (const auto& entry : fs::directory_iterator{cwd})
    {
        ++count;

        // 3. Assert - neither '.' nor '..' should ever be yielded
        auto filename = entry.path().filename().string();
        EXPECT_NE(filename, ".");
        EXPECT_NE(filename, "..");
        EXPECT_TRUE(entry.exists());
    }

    EXPECT_GT(count, 0u);
}

/// @brief Tests that constructing an iterator on an invalid or non-existent path produces an empty range.
TEST(directory_iterator, empty_on_invalid_directory)
{
    // 1. Setup
    auto non_existent = fs::path{"__invalid_non_existent_dir_999"};

    // 2. Act
    auto it = fs::directory_iterator{non_existent};
    auto end_it = fs::directory_iterator{};

    // 3. Assert
    EXPECT_EQ(it, end_it);
}

/// @brief Invariant validation: verifies that cached directory_entry attributes during iteration
/// match standalone filesystem query functions exactly.
TEST(directory_iterator, cached_attributes_match_standalone_queries)
{
    // 1. Setup
    auto cwd = fs::current_path();
    auto count = size_t{0};

    // 2. Act & Assert
    for (const auto& entry : fs::directory_iterator{cwd})
    {
        ++count;
        const auto& p = entry.path();

        EXPECT_EQ(entry.exists(), fs::exists(p));
        EXPECT_EQ(entry.is_directory(), fs::is_directory(p));
        EXPECT_EQ(entry.is_regular_file(), fs::is_regular_file(p));
        EXPECT_EQ(entry.status().type(), fs::status(p).type());

#ifdef _WIN32
        if (entry.is_regular_file())
        {
            EXPECT_EQ(entry.file_size(), fs::file_size(p));
        }
#endif
    }

    EXPECT_GT(count, 0u);
}

// ============================================================================
// filesystem_status & permissions Tests
// ============================================================================

/// @brief Tests attribute-derived Windows permissions and permission bitmask operations.
TEST(filesystem_status, file_permissions)
{
    // 1. Setup
    auto cwd = fs::current_path();

    // 2. Act
    auto st = fs::status(cwd);

    // 3. Assert
    EXPECT_TRUE(fs::status_known(st));
    EXPECT_EQ(st.type(), fs::file_type::directory);

#ifdef _WIN32
    // On Windows, non-readonly directories have read/write/exec permissions
    auto perms = st.perms();
    EXPECT_TRUE((perms & fs::permissions::owner_read) != fs::permissions::none);
    EXPECT_TRUE((perms & fs::permissions::owner_write) != fs::permissions::none);
#endif
}

/// @brief Tests symlink_status observer behavior on normal directories and files.
TEST(filesystem_status, symlink_status_equality)
{
    // 1. Setup
    auto cwd = fs::current_path();

    // 2. Act
    auto direct_status = fs::status(cwd);
    auto sym_status = fs::symlink_status(cwd);

    // 3. Assert - For non-symlink directories, status and symlink_status match
    EXPECT_EQ(direct_status.type(), sym_status.type());
}
