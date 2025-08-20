#include <tempest/filesystem.hpp>

#include <tempest/algorithm.hpp>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>

#include <AclAPI.h>
#include <winioctl.h>
#else
#include <iconv.h>
#include <limits.h>
#include <unistd.h>
#endif

namespace tempest::filesystem
{
    namespace
    {
        template <typename T>
        constexpr T forward_slash = static_cast<T>('/');

        template <typename T>
        constexpr T back_slash = static_cast<T>('\\');

        template <typename T>
        constexpr T dot = static_cast<T>('.');

        template <typename T>
        constexpr T slashes[] = {forward_slash<T>, back_slash<T>};

        template <typename T>
        constexpr T colon = static_cast<T>(':');

        template <typename T>
        constexpr auto is_letter = [](T ch) {
            return (ch >= static_cast<T>('A') && ch <= static_cast<T>('Z')) ||
                   (ch >= static_cast<T>('a') && ch <= static_cast<T>('z'));
        };

        template <typename T>
        constexpr auto is_slash = [](T ch) { return ch == forward_slash<T> || ch == back_slash<T>; };
    } // namespace

    namespace detail
    {
        string convert_wide_to_narrow(tempest::wstring_view wide_str)
        {
            tempest::string result;

#ifdef _WIN32
            // Use WideCharToMultiByte for Windows
            // Use ACP (ANSI Code Page) for conversion
            const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_str.data(), static_cast<int>(wide_str.size()),
                                                         nullptr, 0, nullptr, nullptr);
            result.resize(static_cast<size_t>(size_needed), L'\0');
            WideCharToMultiByte(CP_UTF8, 0, wide_str.data(), static_cast<int>(wide_str.size()), result.data(),
                                static_cast<int>(size_needed), nullptr, nullptr);
#else
            auto conv_desc = iconv_open("UTF-8", "WCHAR_T");
            if (conv_desc == bit_cast<iconv_t>(-1ll))
            {
                perror("iconv_open failed");
                return result;
            }

            auto in_bytes = wide_str.size() * sizeof(wchar_t);
            const auto out_bytes = in_bytes * 4 + 1; // UTF-8 can take up to 4 bytes per character
            result.resize(out_bytes, L'\0');

            char* in_buf = const_cast<char*>(reinterpret_cast<const char*>(wide_str.data()));
            char* out_buf = result.data();

            auto bytes_left = out_bytes;

            const auto res = iconv(conv_desc, &in_buf, &in_bytes, &out_buf, &bytes_left);
            if (res == static_cast<size_t>(-1))
            {
                perror("iconv failed");
                iconv_close(conv_desc);
                return string();
            }

            iconv_close(conv_desc);
            result.resize(out_bytes - bytes_left);
#endif

            return result;
        }

        wstring convert_narrow_to_wide(tempest::string_view narrow_str)
        {
            wstring result;
#ifdef _WIN32
            const auto size_needed =
                MultiByteToWideChar(CP_UTF8, 0, narrow_str.data(), static_cast<int>(narrow_str.size()), nullptr, 0);
            result.resize(static_cast<size_t>(size_needed), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, narrow_str.data(), static_cast<int>(narrow_str.size()), result.data(),
                                static_cast<int>(size_needed));
#else
            auto conv_desc = iconv_open("WCHAR_T", "UTF-8");
            if (conv_desc == bit_cast<iconv_t>(-1ll))
            {
                perror("iconv_open failed");
                return result;
            }

            auto in_bytes = narrow_str.size();
            const auto out_bytes = in_bytes * sizeof(wchar_t) + 1; // WCHAR_T can take up to sizeof(wchar_t) bytes
            result.resize(out_bytes / sizeof(wchar_t), L'\0');

            char* in_buf = const_cast<char*>(narrow_str.data());
            char* out_buf = reinterpret_cast<char*>(result.data());

            auto bytes_left = out_bytes;
            const auto res = iconv(conv_desc, &in_buf, &in_bytes, &out_buf, &bytes_left);
            if (res == static_cast<size_t>(-1))
            {
                perror("iconv failed");
                iconv_close(conv_desc);
                return wstring();
            }

            iconv_close(conv_desc);
            result.resize((out_bytes - bytes_left) / sizeof(wchar_t));
#endif
            return result;
        }

        template <character_type T>
        constexpr int compare_slash_insensitive(basic_string_view<T> lhs, basic_string_view<T> rhs)
        {
            const auto compare_len = tempest::min(lhs.size(), rhs.size());
            for (size_t i = 0; i < compare_len; ++i)
            {
                if (lhs[i] != rhs[i])
                {
                    if (is_slash<T>(lhs[i]) && is_slash<T>(rhs[i]))
                    {
                        continue; // Both are slashes, treat as equal
                    }
                    return lhs[i] < rhs[i] ? -1 : 1;
                }
            }

            if (lhs.size() < rhs.size())
            {
                return -1;
            }
            else if (lhs.size() > rhs.size())
            {
                return 1;
            }
            return 0;
        }
    } // namespace detail

    namespace
    {
        template <character_type T>
        basic_string_view<T> get_root_name(basic_string_view<T> path)
        {
            // Detect UNC paths
            if (path.size() >= 2 && is_slash<T>(path[0]) && path[0] == path[1])
            {
                size_t idx = 0;
                while (idx < path.size() && path[idx] == path[0])
                {
                    ++idx;
                }

                const size_t host_start = idx;

                while (idx < path.size() && !is_slash<T>(path[idx]))
                {
                    ++idx;
                }

                if (host_start == idx)
                {
                    return {}; // Host is malformed
                }

                // Check for another slash and the share name after the host name
                if (idx < path.size() && is_slash<T>(path[idx]))
                {
                    ++idx; // Skip the slash after the host name
                }
                else
                {
                    return {}; // No share name found
                }

                // Find the next slash or the end of the path
                while (idx < path.size() && !is_slash<T>(path[idx]))
                {
                    ++idx;
                }

                return tempest::substr(path, 0, idx);
            }

            // Detect the drive letter and a colon
            if (path.size() >= 2 && is_letter<T>(path[0]) && path[1] == colon<T>)
            {
                return tempest::substr(path, 0, 2);
            }

            // No host name is found
            return {};
        }

        template <character_type T>
        constexpr bool has_root_name(basic_string_view<T> path)
        {
            // Check for UNC paths
            if (path.size() >= 2 && is_slash<T>(path[0]) && path[0] == path[1])
            {
                size_t idx = 0;
                while (idx < path.size() && path[idx] == path[0])
                {
                    ++idx;
                }

                const size_t host_start = idx;
                while (idx < path.size() && !is_slash<T>(path[idx]))
                {
                    ++idx;
                }

                if (host_start == idx)
                {
                    return false; // Host is malformed
                }

                // Check for another slash and the share name after the host name
                if (idx < path.size() && is_slash<T>(path[idx]))
                {
                    ++idx; // Skip the slash after the host name
                }
                else
                {
                    return false; // No share name found
                }

                // Find the next slash or the end of the path
                while (idx < path.size() && !is_slash<T>(path[idx]))
                {
                    ++idx;
                }

                // If we reached the end of the path or found another slash, we have a valid root name
                return idx < path.size() || (idx == path.size() && is_slash<T>(path[path.size() - 1]));
            }

            // Check for drive letter and colon
            return path.size() >= 2 && is_letter<T>(path[0]) && path[1] == colon<T>;
        }

        template <character_type T>
        basic_string_view<T> get_root_directory(basic_string_view<T> path)
        {
            // Detect UNC paths
            if (path.size() >= 2 && is_slash<T>(path[0]) && path[0] == path[1])
            {
                size_t idx = 0;
                while (idx < path.size() && path[idx] == path[0])
                {
                    ++idx;
                }

                // Skip the host name
                while (idx < path.size() && !is_slash<T>(path[idx]))
                {
                    ++idx;
                }

                if (idx < path.size() && is_slash<T>(path[idx]))
                {
                    return tempest::substr(path, idx, 1);
                }

                return {}; // Host is malformed
            }

            if (path.size() >= 2 && is_letter<T>(path[0]) && path[1] == colon<T>)
            {
                size_t idx = 2;
                if (idx < path.size() && is_slash<T>(path[idx]))
                {
                    return tempest::substr(path, idx, 1);
                }
                return {};
            }

            // Handle posix root-only absolute path
            if (!path.empty() && is_slash<T>(path[0]))
            {
                return tempest::substr(path, 0, 1);
            }

            return {};
        }

        template <character_type T>
        constexpr bool has_root_directory(basic_string_view<T> path)
        {
            // Detect UNC paths
            if (path.size() >= 2 && is_slash<T>(path[0]) && path[0] == path[1])
            {
                size_t idx = 0;
                while (idx < path.size() && path[idx] == path[0])
                {
                    ++idx;
                }

                // Skip the host name
                while (idx < path.size() && !is_slash<T>(path[idx]))
                {
                    ++idx;
                }

                if (idx < path.size() && is_slash<T>(path[idx]))
                {
                    return true;
                }

                return false; // Host is malformed
            }

            if (path.size() >= 2 && is_letter<T>(path[0]) && path[1] == colon<T>)
            {
                size_t idx = 2;
                if (idx < path.size() && is_slash<T>(path[idx]))
                {
                    return true;
                }
                return false;
            }

            // Handle posix root-only absolute path
            return !path.empty() && is_slash<T>(path[0]);
        }

        template <typename T>
        basic_string_view<T> get_root_path(basic_string_view<T> path)
        {
            if (path.size() >= 2 && is_slash<T>(path[0]) && path[0] == path[1])
            {
                // UNC path
                size_t idx = 0;
                while (idx < path.size() && is_slash<T>(path[idx]))
                {
                    ++idx;
                }

                // Ensure slashes were skipped
                if (idx < 2)
                {
                    return {};
                }

                // Ensure a host name follows the slashes
                if (idx >= path.size() || !is_letter<T>(path[idx]))
                {
                    return {}; // No host name found
                }

                // Skip the host name
                while (idx < path.size() && !is_slash<T>(path[idx]))
                {
                    ++idx;
                }

                // Ensure there's a trailing slash after the host name
                if (idx < path.size() && is_slash<T>(path[idx]))
                {
                    ++idx; // Skip the slash after the host name
                }
                else
                {
                    return {}; // No share name found
                }

                // Ensure there's a share name after the slash
                if (idx >= path.size() || !is_letter<T>(path[idx]))
                {
                    return {}; // No share name found
                }

                // Skip the share name
                while (idx < path.size() && !is_slash<T>(path[idx]))
                {
                    ++idx;
                }

                // Go to the next slash or the end of the path
                if (idx < path.size() && is_slash<T>(path[idx]))
                {
                    ++idx;
                }

                return tempest::substr(path, 0, idx);
            }

            if (path.size() >= 2 && is_letter<T>(path[0]) && path[1] == colon<T>)
            {
                size_t idx = 2;
                if (idx < path.size() && is_slash<T>(path[idx]))
                {
                    ++idx;
                }
                return tempest::substr(path, 0, idx);
            }

            if (!path.empty() && is_slash<T>(path[0]))
            {
                return tempest::substr(path, 0, 1);
            }

            return {};
        }

        template <character_type T>
        constexpr bool has_root_path(basic_string_view<T> path)
        {
            if (path.size() >= 2 && is_slash<T>(path[0]) && path[0] == path[1])
            {
                // UNC path
                size_t idx = 0;
                while (idx < path.size() && is_slash<T>(path[idx]))
                {
                    ++idx;
                }

                // Ensure there's a host name after the slashes
                if (idx >= path.size() || !is_letter<T>(path[idx]))
                {
                    return false; // No host name found
                }

                // Skip the host name
                while (idx < path.size() && !is_slash<T>(path[idx]))
                {
                    ++idx;
                }

                // Ensure there's a trailing slash after the host name
                if (idx < path.size() && is_slash<T>(path[idx]))
                {
                    ++idx; // Skip the slash after the host name
                }
                else
                {
                    return false; // No share name found
                }

                // If there's a letter after the slash (representing the share name), we have a root path
                return idx < path.size() && is_letter<T>(path[idx]);
            }

            if (path.size() >= 2 && is_letter<T>(path[0]) && path[1] == colon<T>)
            {
                return true; // Drive letter with colon
            }

            return !path.empty() && is_slash<T>(path[0]); // Posix root-only absolute path
        }

        template <character_type T>
        constexpr basic_string_view<T> get_relative_path(basic_string_view<T> path)
        {
            auto root_name = get_root_path<T>(path);
            if (root_name.empty())
            {
                return path;
            }

            // Chop off the root name if it exists
            return substr(path, root_name.size(), path.size() - root_name.size());
        }

        template <character_type T>
        constexpr bool has_relative_path(basic_string_view<T> path)
        {
            if (path.empty())
            {
                return false;
            }

            auto root_name = get_root_path<T>(path);

            // There is a relative path if the root name is empty
            if (root_name.empty())
            {
                return true;
            }

            // If the root name is equal to the path, then the path is the root path
            if (root_name.size() == path.size())
            {
                return false; // The entire path is the root path
            }

            // If the root name is not empty and the path is longer than the root name, then it has a relative path
            return true;
        }

        template <character_type T>
        constexpr basic_string_view<T> get_parent_path(basic_string_view<T> path)
        {
            if (path.empty())
            {
                return {};
            }

            auto end = path.size();
            while (end > 1 && is_slash<T>(path[end - 1]))
            {
                --end;
            }

            if (end == 0)
            {
                return {};
            }

            // Check for Windows style root drives
            if (end == 3 && is_letter<T>(path[0]) && path[1] == colon<T> && is_slash<T>(path[2]))
            {
                return {}; // No parent path for root drive
            }

            // Check for Windows style root drives without trailing slash
            if (end == 2 && is_letter<T>(path[0]) && path[1] == colon<T>)
            {
                return {}; // No parent path for root drive
            }

            // Check for Unix root
            if (end == 1 && is_slash<T>(path[0]))
            {
                return {}; // No parent path for root
            }

            // Check for UNC paths
            if (path.size() >= 5 && is_slash<T>(path[0]) && is_slash<T>(path[1]))
            {
                const auto first = search_first_not_of(path.begin() + 2, path.end(), tempest::begin(slashes<T>),
                                                       tempest::end(slashes<T>));
                if (first != path.end())
                {
                    const auto second =
                        search_first_not_of(first, path.end(), tempest::begin(slashes<T>), tempest::end(slashes<T>));
                    if (second != path.end())
                    {
                        const auto third = search_first_not_of(second, path.end(), tempest::begin(slashes<T>),
                                                               tempest::end(slashes<T>));
                        if (third == path.end() || third > path.begin() + end)
                        {
                            return {}; // No parent path for UNC root
                        }
                    }
                }
            }

            // Get the last slash iterator
            const auto last_slash = tempest::search_last_of(path.begin(), path.begin() + end,
                                                            tempest::begin(slashes<T>), tempest::end(slashes<T>));
            if (last_slash == path.begin() + end)
            {
                return {};
            }

            if (last_slash == path.begin() && is_slash<T>(path[0]))
            {
                return {}; // Root directory has no parent
            }

            return tempest::substr(path, 0, last_slash - path.begin() + 1);
        }

        template <character_type T>
        constexpr bool has_parent_path(basic_string_view<T> path)
        {
            if (path.empty())
            {
                return false;
            }

            auto end = path.size();
            while (end > 1 && is_slash<T>(path[end - 1]))
            {
                --end;
            }

            if (end == 0)
            {
                return false;
            }

            // Check for Windows style root drives
            if (end == 3 && is_letter<T>(path[0]) && path[1] == colon<T> && is_slash<T>(path[2]))
            {
                return false; // No parent path for root drive
            }

            // Check for Windows style root drives without trailing slash
            if (end == 2 && is_letter<T>(path[0]) && path[1] == colon<T>)
            {
                return false; // No parent path for root drive
            }

            // Check for Unix root
            if (end == 1 && is_slash<T>(path[0]))
            {
                return false; // No parent path for root
            }

            // Check for UNC paths
            if (path.size() >= 5 && is_slash<T>(path[0]) && is_slash<T>(path[1]))
            {
                const auto first = search_first_not_of(path.begin() + 2, path.end(), tempest::begin(slashes<T>),
                                                       tempest::end(slashes<T>));
                if (first != path.end())
                {
                    const auto second =
                        search_first_not_of(first, path.end(), tempest::begin(slashes<T>), tempest::end(slashes<T>));
                    if (second != path.end())
                    {
                        const auto third = search_first_not_of(second, path.end(), tempest::begin(slashes<T>),
                                                               tempest::end(slashes<T>));
                        if (third == path.end() || third > path.begin() + end)
                        {
                            return false; // No parent path for UNC root
                        }
                    }
                }
            }

            // Get the last slash iterator
            const auto last_slash = tempest::search_last_of(path.begin(), path.begin() + end,
                                                            tempest::begin(slashes<T>), tempest::end(slashes<T>));
            if (last_slash == path.begin() + end)
            {
                return false;
            }

            if (last_slash == path.begin() && is_slash<T>(path[0]))
            {
                return false; // Root directory has no parent
            }

            return true;
        }

        template <character_type T>
        constexpr basic_string_view<T> get_filename(basic_string_view<T> path)
        {
            if (path.empty())
            {
                return {};
            }

            // Trim the trailing slashes
            auto trimmed = path;
            if (!trimmed.empty() && is_slash<T>(trimmed.back()))
            {
                trimmed = tempest::substr(trimmed, 0, trimmed.size() - 1);
            }

            // Handle unix root directory
            if (trimmed.empty())
            {
                return {};
            }

            // Handle windows roots
            if (trimmed.size() == 2 && is_letter<T>(trimmed[0]) && trimmed[1] == colon<T>)
            {
                return {}; // No filename for root drive
            }

            // Handle UNC detection
            if (trimmed.size() >= 2 && is_slash<T>(trimmed[0]) && trimmed[0] == trimmed[1])
            {
                auto pos =
                    search_first_of(path.begin() + 2, path.end(), tempest::begin(slashes<T>), tempest::end(slashes<T>));
                if (pos != trimmed.end())
                {
                    auto next_slash =
                        search_first_of(pos + 1, trimmed.end(), tempest::begin(slashes<T>), tempest::end(slashes<T>));
                    if (next_slash == trimmed.end())
                    {
                        return {}; // No filename found after UNC share name
                    }
                }
            }

            auto pos =
                search_last_of(trimmed.begin(), trimmed.end(), tempest::begin(slashes<T>), tempest::end(slashes<T>));
            if (pos == trimmed.end())
            {
                return trimmed; // No slashes found, the entire path is the filename
            }

            return tempest::substr(trimmed, pos - trimmed.begin() + 1, trimmed.size() - (pos - trimmed.begin() + 1));
        }

        template <character_type T>
        constexpr bool has_filename(basic_string_view<T> path)
        {
            if (path.empty())
            {
                return false;
            }

            // Trim the trailing slashes
            auto trimmed = path;
            if (!trimmed.empty() && is_slash<T>(trimmed.back()))
            {
                trimmed = tempest::substr(trimmed, 0, trimmed.size() - 1);
            }

            // Handle unix root directory
            if (trimmed.empty())
            {
                return false;
            }

            // Handle windows roots
            if (trimmed.size() == 2 && is_letter<T>(trimmed[0]) && trimmed[1] == colon<T>)
            {
                return false; // No filename for root drive
            }

            // Handle UNC detection
            if (trimmed.size() >= 2 && is_slash<T>(trimmed[0]) && trimmed[0] == trimmed[1])
            {
                auto pos =
                    search_first_of(path.begin() + 2, path.end(), tempest::begin(slashes<T>), tempest::end(slashes<T>));
                if (pos != trimmed.end())
                {
                    auto next_slash =
                        search_first_of(pos + 1, trimmed.end(), tempest::begin(slashes<T>), tempest::end(slashes<T>));
                    if (next_slash == trimmed.end())
                    {
                        return false; // No filename found after UNC share name
                    }
                }
            }

            return true;
        }

        template <character_type T>
        constexpr basic_string_view<T> get_stem(basic_string_view<T> path)
        {
            auto filename = get_filename<T>(path);
            if (filename.empty())
            {
                return {};
            }

            // Find the last dot in the filename
            auto last_dot = search_last_of(filename.begin(), filename.end(), dot<T>);
            if (last_dot == filename.end())
            {
                return filename; // No dot found, return the entire filename
            }

            // If the last dot is the first character, it means the filename starts with a dot
            if (last_dot == filename.begin())
            {
                return filename;
            }

            // Return the substring before the last dot
            return tempest::substr(filename, 0, last_dot - filename.begin());
        }

        template <character_type T>
        constexpr bool has_stem(basic_string_view<T> path)
        {
            auto filename = get_filename<T>(path);
            if (filename.empty())
            {
                return false;
            }

            // Find the last dot in the filename
            auto last_dot = search_last_of(filename.begin(), filename.end(), dot<T>);
            if (last_dot == filename.end())
            {
                return true; // No dot found, stem exists
            }

            return true;
        }

        template <character_type T>
        constexpr basic_string_view<T> get_extension(basic_string_view<T> path)
        {
            auto filename = get_filename<T>(path);
            if (filename.empty())
            {
                return {};
            }

            // Find the last dot in the filename
            auto last_dot = search_last_of(filename.begin(), filename.end(), dot<T>);
            if (last_dot == filename.end() || last_dot == filename.begin())
            {
                return {}; // No extension found or the dot is the first character
            }

            // Return the substring from the last dot to the end of the filename
            return tempest::substr(filename, last_dot - filename.begin(),
                                   filename.size() - (last_dot - filename.begin()));
        }

        template <character_type T>
        constexpr bool has_extension(basic_string_view<T> path)
        {
            auto filename = get_filename<T>(path);
            if (filename.empty())
            {
                return false;
            }

            // Find the last dot in the filename
            auto last_dot = search_last_of(filename.begin(), filename.end(), dot<T>);
            if (last_dot == filename.end() || last_dot == filename.begin())
            {
                return false; // No extension found or the dot is the first character
            }

            return true; // Extension exists if the last dot is not the first character
        }

        template <character_type T>
        constexpr T detect_path_separator(basic_string_view<T> path)
        {
            if (path.empty())
            {
                return path::preferred_separator;
            }

            size_t forward_slash_count = 0;
            size_t back_slash_count = 0;

            for (const auto& ch : path)
            {
                if (ch == forward_slash<T>)
                {
                    ++forward_slash_count;
                }
                else if (ch == back_slash<T>)
                {
                    ++back_slash_count;
                }
            }

            // If there are purely forward slashes, return forward slash
            // If there are purely back slashes, return back slash
            // If there are both, return the preferred separator

            if (forward_slash_count > 0 && back_slash_count == 0)
            {
                return forward_slash<T>;
            }
            else if (back_slash_count > 0 && forward_slash_count == 0)
            {
                return back_slash<T>;
            }
            else
            {
                return path::preferred_separator; // Default to preferred separator
            }
        }
    } // namespace

    path& path::assign(const path& p)
    {
        _path = p._path;
        return *this;
    }

    path& path::assign(path&& p) noexcept
    {
        _path = tempest::move(p._path);
        return *this;
    }

    path& path::assign(string_type&& p) noexcept
    {
        _path = tempest::move(p);
        return *this;
    }

    path& path::operator+=(const path& p)
    {
        _path.append(p._path);
        return *this;
    }

    path& path::operator+=(const string_type& p)
    {
        _path.append(p);
        return *this;
    }

    path& path::operator+=(basic_string_view<value_type> p)
    {
        _path.append(p.data(), p.size());
        return *this;
    }

    path& path::operator+=(const value_type* p)
    {
        _path.append(p);
        return *this;
    }

    path& path::operator+=(value_type ch)
    {
        _path.push_back(ch);
        return *this;
    }

    const path::value_type* path::c_str() const noexcept
    {
        return _path.c_str();
    }

    const path::string_type& path::native() const noexcept
    {
        return _path;
    }

    path::operator string_type() const
    {
        return _path;
    }

    tempest::string path::string() const
    {
#ifdef _WIN32
        return detail::convert_wide_to_narrow(_path);
#else
        return _path;
#endif
    }

    tempest::wstring path::wstring() const
    {
#ifdef _WIN32
        return _path;
#else
        return detail::convert_narrow_to_wide(_path);
#endif
    }

    tempest::string path::generic_string() const
    {
        auto str = string();
        replace(str.begin(), str.end(), '\\', '/');
        return str;
    }

    tempest::wstring path::generic_wstring() const
    {
        auto wstr = wstring();
        replace(wstr.begin(), wstr.end(), L'\\', L'/');
        return wstr;
    }

    path path::root_name() const
    {
        path p = get_root_name<value_type>(_path);
        return p;
    }

    path path::root_directory() const
    {
        path p = get_root_directory<value_type>(_path);
        return p;
    }

    path path::root_path() const
    {
        path p = get_root_path<value_type>(_path);
        return p;
    }

    path path::relative_path() const
    {
        path p = get_relative_path<value_type>(_path);
        return p;
    }

    path path::parent_path() const
    {
        path p = get_parent_path<value_type>(_path);
        return p;
    }

    path path::filename() const
    {
        path p = get_filename<value_type>(_path);
        return p;
    }

    path path::stem() const
    {
        path p = get_stem<value_type>(_path);
        return p;
    }

    path path::extension() const
    {
        path p = get_extension<value_type>(_path);
        return p;
    }

    bool path::empty() const
    {
        return _path.empty();
    }

    bool path::has_root_path() const
    {
        return ::tempest::filesystem::has_root_path<value_type>(_path);
    }

    bool path::has_root_name() const
    {
        return ::tempest::filesystem::has_root_name<value_type>(_path);
    }

    bool path::has_root_directory() const
    {
        return ::tempest::filesystem::has_root_directory<value_type>(_path);
    }

    bool path::has_relative_path() const
    {
        return ::tempest::filesystem::has_relative_path<value_type>(_path);
    }

    bool path::has_parent_path() const
    {
        return ::tempest::filesystem::has_parent_path<value_type>(_path);
    }

    bool path::has_filename() const
    {
        return ::tempest::filesystem::has_filename<value_type>(_path);
    }

    bool path::has_stem() const
    {
        return ::tempest::filesystem::has_stem<value_type>(_path);
    }

    bool path::has_extension() const
    {
        return ::tempest::filesystem::has_extension<value_type>(_path);
    }

    bool path::is_absolute() const
    {
        return has_root_path();
    }

    bool path::is_relative() const
    {
        return !is_absolute();
    }

    path& path::_append(const path& p)
    {
        if (p.empty())
        {
            return *this; // Nothing to append
        }

        if (empty())
        {
            _path = p._path; // If current path is empty, just assign the new path
            return *this;
        }

        // If p.is_absolute() || (p.has_root_name() && p.root_name() != root_name())
        // Then we replace the current path with p._path
        //
        // - If p.has_root_directory(), remove any root directory and relative path from the generic pathname
        // - If !has_root_directory() && is_absolute() || has_filename(), append preferred_separator
        // - Append native format of p, omitting root name
        if (p.is_absolute() || (p.has_root_name() && (p.root_name() != root_name())))
        {
            _path = p._path;
        }
        else
        {
            auto separator = detect_path_separator<value_type>(_path);

            if (p.has_root_directory())
            {
                // Remove the root directory + relative path from *this
                _path = root_name();
            }
            else if ((!has_root_directory() && is_absolute()) || has_filename())
            {
                // If we don't have a root directory and the path is absolute, or we have a filename,
                // append the preferred separator
                if (!_path.empty() && !is_slash<value_type>(_path.back()))
                {
                    _path.push_back(separator);
                }
            }

            auto rel_path = p.relative_path();
            if (!rel_path.empty() && !is_slash<value_type>(_path.back()))
            {
                _path.push_back(separator); // Ensure separator before appending
            }
            _path.append(rel_path._path);
        }

        return *this;
    }

    bool operator==(const path& lhs, const path& rhs) noexcept
    {
        return detail::compare_slash_insensitive<path::value_type>(lhs.native(), rhs.native()) == 0;
    }

    bool operator!=(const path& lhs, const path& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    bool operator<(const path& lhs, const path& rhs) noexcept
    {
        return detail::compare_slash_insensitive<path::value_type>(lhs.native(), rhs.native()) < 0;
    }

    bool operator<=(const path& lhs, const path& rhs) noexcept
    {
        return detail::compare_slash_insensitive<path::value_type>(lhs.native(), rhs.native()) <= 0;
    }

    bool operator>(const path& lhs, const path& rhs) noexcept
    {
        return detail::compare_slash_insensitive<path::value_type>(lhs.native(), rhs.native()) > 0;
    }

    bool operator>=(const path& lhs, const path& rhs) noexcept
    {
        return detail::compare_slash_insensitive<path::value_type>(lhs.native(), rhs.native()) >= 0;
    }

    file_status::file_status() noexcept : file_status(file_type::none)
    {
    }

    file_status::file_status(file_type type, permissions perms) noexcept : _type{type}, _permissions{perms}
    {
    }

    bool is_block_file(const file_status& status)
    {
        return status.type() == file_type::block;
    }

    bool is_block_file(const path& p)
    {
        return is_block_file(status(p));
    }

    bool is_character_file(const file_status& status)
    {
        return status.type() == file_type::character;
    }

    bool is_character_file(const path& p)
    {
        return is_character_file(status(p));
    }

    bool is_directory(const file_status& status)
    {
        return status.type() == file_type::directory;
    }

    bool is_directory(const path& p)
    {
        return is_directory(status(p));
    }

    bool is_empty(const path& p)
    {
        return p.empty() || (p.has_root_path() && p.relative_path().empty());
    }

    bool is_fifo(const file_status& status)
    {
        return status.type() == file_type::fifo;
    }

    bool is_fifo(const path& p)
    {
        return is_fifo(status(p));
    }

    bool is_other(const file_status& status)
    {
        return exists(status) && !is_regular_file(status) && !is_directory(status) && !is_symlink(status);
    }

    bool is_other(const path& p)
    {
        return is_other(status(p));
    }

    bool is_regular_file(const file_status& status)
    {
        return status.type() == file_type::regular;
    }

    bool is_regular_file(const path& p)
    {
        return is_regular_file(status(p));
    }

    bool is_socket(const file_status& status)
    {
        return status.type() == file_type::socket;
    }

    bool is_socket(const path& p)
    {
        return is_socket(status(p));
    }

    bool is_symlink(const file_status& status)
    {
        return status.type() == file_type::symlink;
    }

    bool is_symlink(const path& p)
    {
        return is_symlink(status(p));
    }

    bool status_known(const file_status& status)
    {
        return status.type() != file_type::none;
    }

    namespace
    {
#ifdef _WIN32
        // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_reparse_data_buffer
        typedef struct _REPARSE_DATA_BUFFER
        {
            ULONG ReparseTag;
            USHORT ReparseDataLength;
            USHORT Reserved;
            union {
                struct
                {
                    USHORT SubstituteNameOffset;
                    USHORT SubstituteNameLength;
                    USHORT PrintNameOffset;
                    USHORT PrintNameLength;
                    ULONG Flags;
                    WCHAR PathBuffer[1];
                } SymbolicLinkReparseBuffer;
                struct
                {
                    USHORT SubstituteNameOffset;
                    USHORT SubstituteNameLength;
                    USHORT PrintNameOffset;
                    USHORT PrintNameLength;
                    WCHAR PathBuffer[1];
                } MountPointReparseBuffer;
                struct
                {
                    UCHAR DataBuffer[1];
                } GenericReparseBuffer;
            } DUMMYUNIONNAME;
        } REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

        path::string_type follow_symlink(const path::string_type& p)
        {
            HANDLE h = CreateFileW(p.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                   OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);

            if (h == INVALID_HANDLE_VALUE)
            {
                return path::string_type{};
            }

            auto buf = vector<byte>(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
            DWORD bytes_returned = 0;
            BOOL ok = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, nullptr, 0, buf.data(),
                                      static_cast<DWORD>(buf.size()), &bytes_returned, nullptr);
            CloseHandle(h);

            if (!ok)
            {
                return path::string_type{};
            }

            auto rdb = reinterpret_cast<REPARSE_DATA_BUFFER*>(buf.data());

            if (rdb->ReparseTag == IO_REPARSE_TAG_SYMLINK)
            {
                auto& symlink = rdb->SymbolicLinkReparseBuffer;
                return path::string_type(symlink.PathBuffer + symlink.PrintNameOffset / sizeof(WCHAR),
                                         symlink.PrintNameLength / sizeof(WCHAR));
            }

            if (rdb->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
            {
                auto& mount_point = rdb->MountPointReparseBuffer;
                return path::string_type(mount_point.PathBuffer + mount_point.PrintNameOffset / sizeof(WCHAR),
                                         mount_point.PrintNameLength / sizeof(WCHAR));
            }

            return p;
        }

        file_type get_file_type(const path::string_type& p)
        {
            WIN32_FILE_ATTRIBUTE_DATA file_info;
            if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &file_info))
            {
                DWORD err = GetLastError();
                if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
                {
                    return file_type::not_found; // File or path does not exist
                }
                return file_type::unknown; // Unknown error, treat as unknown type
            }

            if (file_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            {
                return file_type::symlink; // Reparse point indicates a symlink
            }
            if (file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                return file_type::directory; // Directory
            }
            if (file_info.dwFileAttributes & FILE_ATTRIBUTE_DEVICE)
            {
                return file_type::character; // Device file
            }

            return file_type::regular;
        }

        enum_mask<permissions> get_permissions(const path::string_type& p)
        {
            PSECURITY_DESCRIPTOR sec_desc = nullptr;
            PSID owner = nullptr;
            PSID group = nullptr;
            PACL dacl = nullptr;

            const auto res = GetNamedSecurityInfoW(p.c_str(), SE_FILE_OBJECT,
                                                   OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                                                       DACL_SECURITY_INFORMATION,
                                                   &owner, &group, &dacl, nullptr, &sec_desc);

            if (res != ERROR_SUCCESS)
            {
                return enum_mask(permissions::unknown);
            }

            if (dacl == nullptr)
            {
                return enum_mask(permissions::unknown); // No DACL present
            }

            auto perms = enum_mask(permissions::none);

            for (DWORD i = 0; i < dacl->AceCount; ++i)
            {
                void* ace;

                if (!GetAce(dacl, i, &ace))
                {
                    continue; // Skip if we can't get the ACE
                }

                ACE_HEADER* ace_header = static_cast<ACE_HEADER*>(ace);
                if (ace_header->AceType != ACCESS_ALLOWED_ACE_TYPE)
                {
                    continue; // Only interested in ACCESS_ALLOWED_ACE_TYPE
                }

                ACCESS_ALLOWED_ACE* allowed_ace = static_cast<ACCESS_ALLOWED_ACE*>(ace);
                PSID sid = &allowed_ace->SidStart;
                DWORD mask = allowed_ace->Mask;

                if (EqualSid(sid, owner))
                {
                    if (mask & (FILE_GENERIC_READ | GENERIC_READ | FILE_READ_DATA))
                    {
                        perms |= permissions::owner_read;
                    }
                    if (mask & (FILE_GENERIC_WRITE | GENERIC_WRITE | FILE_WRITE_DATA))
                    {
                        perms |= permissions::owner_write;
                    }
                    if (mask & (FILE_GENERIC_EXECUTE | GENERIC_EXECUTE | FILE_EXECUTE))
                    {
                        perms |= permissions::owner_execute;
                    }
                }
                else if (EqualSid(sid, group))
                {
                    if (mask & (FILE_GENERIC_READ | GENERIC_READ | FILE_READ_DATA))
                    {
                        perms |= permissions::group_read;
                    }
                    if (mask & (FILE_GENERIC_WRITE | GENERIC_WRITE | FILE_WRITE_DATA))
                    {
                        perms |= permissions::group_write;
                    }
                    if (mask & (FILE_GENERIC_EXECUTE | GENERIC_EXECUTE | FILE_EXECUTE))
                    {
                        perms |= permissions::group_execute;
                    }
                }
                else
                {
                    SID_IDENTIFIER_AUTHORITY others_auth = SECURITY_WORLD_SID_AUTHORITY;
                    PSID others_sid = nullptr;
                    if (AllocateAndInitializeSid(&others_auth, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &others_sid))
                    {
                        bool is_others = others_sid && EqualSid(sid, others_sid);
                        FreeSid(others_sid);

                        if (is_others)
                        {
                            if (mask & (FILE_GENERIC_READ | GENERIC_READ | FILE_READ_DATA))
                            {
                                perms |= permissions::others_read;
                            }

                            if (mask & (FILE_GENERIC_WRITE | GENERIC_WRITE | FILE_WRITE_DATA))
                            {
                                perms |= permissions::others_write;
                            }

                            if (mask & (FILE_GENERIC_EXECUTE | GENERIC_EXECUTE | FILE_EXECUTE))
                            {
                                perms |= permissions::others_execute;
                            }
                        }
                    }
                }
            }

            return perms;
        }
#else
        file_type model_to_file_type(mode_t mode)
        {
            if (S_ISREG(mode))
            {
                return file_type::regular;
            }

            if (S_ISDIR(mode))
            {
                return file_type::directory;
            }

            if (S_ISLNK(mode))
            {
                return file_type::symlink;
            }

            if (S_ISBLK(mode))
            {
                return file_type::block;
            }

            if (S_ISCHR(mode))
            {
                return file_type::character;
            }

            if (S_ISFIFO(mode))
            {
                return file_type::fifo;
            }

            if (S_ISSOCK(mode))
            {
                return file_type::socket;
            }

            return file_type::unknown;
        }

        file_status get_unix_file_status(const path::string_type& p, bool follow_link)
        {
            if (follow_link)
            {
                struct stat file_stat;
                if (::stat(p.c_str(), &file_stat) != 0)
                {
                    if (errno == ENOENT)
                    {
                        return file_status(file_type::not_found);
                    }

                    return file_status(file_type::unknown);
                }

                return file_status(model_to_file_type(file_stat.st_mode),
                                   static_cast<permissions>(file_stat.st_mode & 07777));
            }

            struct stat file_stat;
            if (::lstat(p.c_str(), &file_stat) != 0)
            {
                if (errno == ENOENT)
                {
                    return file_status(file_type::not_found);
                }
                return file_status(file_type::unknown);
            }
            return file_status(model_to_file_type(file_stat.st_mode),
                               static_cast<permissions>(file_stat.st_mode & 07777));
        }
#endif
    } // namespace

    file_status status(const path& p)
    {
#ifdef _WIN32
        const auto native_path = follow_symlink(p.native());
        auto type = get_file_type(native_path);
        auto perms = get_permissions(native_path);
        return file_status(type, perms);
#else
        return get_unix_file_status(p.native(), true);
#endif
    }

    file_status symlink_status(const path& p)
    {
#ifdef _WIN32
        const auto native_path = p.native();
        auto type = get_file_type(native_path);
        auto perms = get_permissions(native_path);
        return file_status(type, perms);
#else
        return get_unix_file_status(p.native(), false);
#endif
    }

    bool exists(const file_status& status)
    {
        return status.type() != file_type::not_found && status_known(status);
    }
} // namespace tempest::filesystem