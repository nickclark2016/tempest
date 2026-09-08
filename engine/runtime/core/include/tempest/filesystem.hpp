#ifndef tempest_filesystem_hpp
#define tempest_filesystem_hpp

#include <tempest/api.hpp>
#include <tempest/charconv.hpp>
#include <tempest/concepts.hpp>
#include <tempest/enum.hpp>
#include <tempest/iterator.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/type_traits.hpp>

namespace tempest::filesystem
{
    using tempest::operator|;
    using tempest::operator&;
    using tempest::operator^;
    using tempest::operator~;

    namespace detail
    {
#ifdef _WIN32
        using native_path_char_type = wchar_t;
        static constexpr auto native_path_separator = L'\\';
#else
        using native_path_char_type = char;
        static constexpr auto native_path_separator = '/';
#endif

        /// <summary>
        /// Concept that checks if a type is a valid source type for constructing a path.
        /// The type must be:
        /// * Specialization of basic_string OR
        /// * Specialization of basic_string_view OR
        /// * Value type of the iterator traits must be valid and denote a possible const qualified character type
        /// </summary>
        /// <typeparam name="T">The type to check.</typeparam>
        template <typename T>
        concept path_source_type =
            (is_specialization_v<T, basic_string> || is_specialization_v<T, basic_string_view> ||
             (is_convertible_v<T, basic_string_view<char>> || is_convertible_v<T, basic_string_view<wchar_t>>));

        template <typename T>
        struct fs_char_type;

        template <typename T>
            requires requires { typename remove_cvref_t<T>::value_type; }
        struct fs_char_type<T>
        {
            using type = remove_const_t<typename remove_cvref_t<T>::value_type>;
        };

        template <character_type T>
        struct fs_char_type<T*>
        {
            using type = remove_const_t<remove_pointer_t<remove_cvref_t<T>>>;
        };

        template <character_type T, size_t N>
        struct fs_char_type<T[N]>
        {
            using type = remove_const_t<remove_extent_t<remove_cvref_t<T>>>;
        };

        template <typename T>
        auto convert_to_native(const T& p)
        {
            using char_type = fs_char_type<T>::type;

            if constexpr (is_same_v<char_type, native_path_char_type>)
            {
                return p; // Already in native format
            }
            else if constexpr (is_same_v<char_type, char>)
            {
                static_assert(is_same_v<native_path_char_type, wchar_t>, "native_path_char_type must be a wchar_t");
                // The type is not the native path type and is char, so it must be converted to wide string
                return tempest::convert_narrow_to_wide(tempest::string_view(p));
            }
            else if constexpr (is_same_v<char_type, wchar_t>)
            {
                static_assert(is_same_v<native_path_char_type, char>, "native_path_char_type must be a char");
                // The type is not the native path type and is wchar_t, so it must be converted to narrow string
                return tempest::convert_wide_to_narrow(tempest::wstring_view(p));
            }
            else
            {
                static_assert(!is_same_v<char_type, char_type>, "Unsupported character type for path conversion.");
            }
        }
    } // namespace detail

    class path;
    class file_status;

    class TEMPEST_API path_iterator
    {
      public:
        using value_type = basic_string_view<detail::native_path_char_type>;
        using difference_type = ptrdiff_t;

        path_iterator() noexcept;
        explicit path_iterator(value_type path);

        auto operator++() -> path_iterator&;
        auto operator++(int) -> path_iterator;

        auto operator*() const noexcept -> value_type;

      private:
        value_type _full;
        size_t _offset = 0;
        size_t _length = 0;

        friend auto operator==(const path_iterator& lhs, const path_iterator& rhs) noexcept -> bool;
        friend auto operator!=(const path_iterator& lhs, const path_iterator& rhs) noexcept -> bool;
    };

    inline auto operator==(const path_iterator& lhs, const path_iterator& rhs) noexcept -> bool
    {
        return lhs._offset == rhs._offset && lhs._length == rhs._length;
    }

    inline auto operator!=(const path_iterator& lhs, const path_iterator& rhs) noexcept -> bool
    {
        return !(lhs == rhs);
    }

    class TEMPEST_API path
    {
      public:
        using value_type = detail::native_path_char_type;
        using string_type = tempest::basic_string<value_type>;
        using const_iterator = path_iterator;
        using iterator = const_iterator;

        static constexpr value_type preferred_separator = detail::native_path_separator;

        path() noexcept = default;

        path(const path&) = default;

        path(path&&) noexcept = default;

        path(const value_type* p) : _path{p}
        {
        }

        path(const string_type& p) : _path{p}
        {
        }

        path(string_type&& p) noexcept : _path{tempest::move(p)}
        {
        }

        path(basic_string_view<value_type> p) : _path{p}
        {
        }

        path(nullptr_t) = delete;

        template <typename T>
            requires(!is_same_v<T, path> && detail::path_source_type<T>)
        path(const T& p)
        {
            assign(p);
        }

        ~path() noexcept = default;

        auto operator=(const path& rhs) -> path& = default;

        auto operator=(path&& rhs) noexcept -> path&
        {
            _path = tempest::move(rhs._path);
            return *this;
        }

        auto operator=(string_type&& rhs) noexcept -> path&
        {
            _path = tempest::move(rhs);
            return *this;
        }

        auto operator=(nullptr_t) -> path& = delete;

        template <detail::path_source_type T>
        auto operator=(const T& t) -> path&
        {
            return assign(t);
        }

        auto assign(const path& p) -> path&;
        auto assign(path&& p) noexcept -> path&;
        auto assign(string_type&& p) noexcept -> path&;

        template <detail::path_source_type T>
        auto assign(const T& p) -> path&
        {
            _path = detail::convert_to_native(p);
            return *this;
        }

        template <detail::path_source_type T>
        auto append(const T& p) -> path&
        {
            return _append(path(p));
        }

        template <detail::path_source_type T>
        auto operator/=(const T& p) -> path&
        {
            return _append(path(p));
        }

        template <detail::path_source_type T>
        auto concat(const T& p) -> path&
        {
            auto native = detail::convert_to_native(p);
            _path.append(tempest::move(native));
            return *this;
        }

        template <detail::path_source_type T>
        auto operator+=(const T& p) -> path&
        {
            return concat(p);
        }

        template <character_type T>
        auto operator+=(T x) -> path&
        {
            return concat(tempest::basic_string_view<value_type>(&x, 1));
        }

        auto operator+=(const path& p) -> path&;
        auto operator+=(const string_type& p) -> path&;
        auto operator+=(basic_string_view<value_type> p) -> path&;
        auto operator+=(const value_type* p) -> path&;
        auto operator+=(value_type ch) -> path&;

        void clear();
        auto make_preferred() -> path&;
        auto remove_filename() -> path&;
        auto replace_filename(const path& replacement) -> path&;
        auto replace_extension(const path& replacement) -> path&;
        void swap(path& other) noexcept;

        [[nodiscard]] auto c_str() const noexcept -> const value_type*;
        [[nodiscard]] auto native() const noexcept -> const string_type&;
        [[nodiscard]] operator string_type() const;

        [[nodiscard]] auto string() const -> tempest::string;
        [[nodiscard]] auto wstring() const -> tempest::wstring;
        [[nodiscard]] auto generic_string() const -> tempest::string;
        [[nodiscard]] auto generic_wstring() const -> tempest::wstring;

        [[nodiscard]] auto root_name() const -> path;
        [[nodiscard]] auto root_directory() const -> path;
        [[nodiscard]] auto root_path() const -> path;
        [[nodiscard]] auto relative_path() const -> path;
        [[nodiscard]] auto parent_path() const -> path;
        [[nodiscard]] auto filename() const -> path;
        [[nodiscard]] auto stem() const -> path;
        [[nodiscard]] auto extension() const -> path;

        [[nodiscard]] auto empty() const -> bool;
        [[nodiscard]] auto has_root_path() const -> bool;
        [[nodiscard]] auto has_root_name() const -> bool;
        [[nodiscard]] auto has_root_directory() const -> bool;
        [[nodiscard]] auto has_relative_path() const -> bool;
        [[nodiscard]] auto has_parent_path() const -> bool;
        [[nodiscard]] auto has_filename() const -> bool;
        [[nodiscard]] auto has_stem() const -> bool;
        [[nodiscard]] auto has_extension() const -> bool;
        [[nodiscard]] auto is_absolute() const -> bool;
        [[nodiscard]] auto is_relative() const -> bool;

        [[nodiscard]] auto begin() const -> iterator;
        [[nodiscard]] auto cbegin() const -> const_iterator;
        [[nodiscard]] auto end() const -> iterator;
        [[nodiscard]] auto cend() const -> const_iterator;

      private:
        string_type _path;

        auto _append(const path& p) -> path&;

        friend auto operator/(const path& lhs, const path& rhs) noexcept -> path;
    };

    TEMPEST_API auto operator==(const path& lhs, const path& rhs) noexcept -> bool;
    TEMPEST_API auto operator!=(const path& lhs, const path& rhs) noexcept -> bool;
    TEMPEST_API auto operator<(const path& lhs, const path& rhs) noexcept -> bool;
    TEMPEST_API auto operator<=(const path& lhs, const path& rhs) noexcept -> bool;
    TEMPEST_API auto operator>(const path& lhs, const path& rhs) noexcept -> bool;
    TEMPEST_API auto operator>=(const path& lhs, const path& rhs) noexcept -> bool;

    inline auto operator/(const path& lhs, const path& rhs) noexcept -> path
    {
        auto result = lhs;
        result._append(rhs);
        return result;
    }

    enum class directory_options
    {
        none = 0,
        follow_directory_symlink = 0x1,
        skip_permissions_denied = 0x2
    };

    enum class file_type
    {
        none,
        not_found,
        regular,
        directory,
        symlink,
        block,
        character,
        fifo,
        socket,
        unknown,
    };

    enum class permissions
    {
        none = 0,
        owner_read = 0400,
        owner_write = 0200,
        owner_execute = 0100,
        owner_all = owner_read | owner_write | owner_execute,
        group_read = 0040,
        group_write = 0020,
        group_execute = 0010,
        group_all = group_read | group_write | group_execute,
        others_read = 0004,
        others_write = 0002,
        others_execute = 0001,
        others_all = others_read | others_write | others_execute,
        all = owner_all | group_all | others_all,
        set_uid = 04000,
        set_gid = 02000,
        sticky_bit = 01000,
        mask = 07777,
        unknown = 0xFFFF
    };

    class TEMPEST_API file_status
    {
      public:
        file_status() noexcept;
        explicit file_status(file_type type, permissions perms = permissions::unknown) noexcept;

        file_status(const file_status& other) noexcept = default;
        file_status(file_status&& other) noexcept = default;

        ~file_status() noexcept = default;

        auto operator=(const file_status& other) noexcept -> file_status& = default;
        auto operator=(file_status&& other) noexcept -> file_status& = default;

        [[nodiscard]] auto type() const noexcept -> file_type
        {
            return _type;
        }

        void type(file_type t) noexcept
        {
            _type = t;
        }

        [[nodiscard]] auto perms() const noexcept -> permissions
        {
            return _permissions;
        }

        void perms(permissions p) noexcept
        {
            _permissions = p;
        }

      private:
        file_type _type = file_type::unknown;
        permissions _permissions = permissions::unknown;
    };

    [[nodiscard]] TEMPEST_API auto is_block_file(const file_status& status) -> bool;
    [[nodiscard]] TEMPEST_API auto is_block_file(const path& p) -> bool;
    [[nodiscard]] TEMPEST_API auto is_character_file(const file_status& status) -> bool;
    [[nodiscard]] TEMPEST_API auto is_character_file(const path& p) -> bool;
    [[nodiscard]] TEMPEST_API auto is_directory(const file_status& status) -> bool;
    [[nodiscard]] TEMPEST_API auto is_directory(const path& p) -> bool;
    [[nodiscard]] TEMPEST_API auto is_empty(const path& p) -> bool;
    [[nodiscard]] TEMPEST_API auto is_fifo(const file_status& status) -> bool;
    [[nodiscard]] TEMPEST_API auto is_fifo(const path& p) -> bool;
    [[nodiscard]] TEMPEST_API auto is_other(const file_status& status) -> bool;
    [[nodiscard]] TEMPEST_API auto is_other(const path& p) -> bool;
    [[nodiscard]] TEMPEST_API auto is_regular_file(const file_status& status) -> bool;
    [[nodiscard]] TEMPEST_API auto is_regular_file(const path& p) -> bool;
    [[nodiscard]] TEMPEST_API auto is_socket(const file_status& status) -> bool;
    [[nodiscard]] TEMPEST_API auto is_socket(const path& p) -> bool;
    [[nodiscard]] TEMPEST_API auto is_symlink(const file_status& status) -> bool;
    [[nodiscard]] TEMPEST_API auto is_symlink(const path& p) -> bool;
    [[nodiscard]] TEMPEST_API auto status_known(const file_status& status) -> bool;

    [[nodiscard]] TEMPEST_API auto status(const path& p) -> file_status;
    [[nodiscard]] TEMPEST_API auto symlink_status(const path& p) -> file_status;

    [[nodiscard]] TEMPEST_API auto exists(const file_status& status) -> bool;
    [[nodiscard]] TEMPEST_API auto exists(const path& p) -> bool;

    [[nodiscard]] TEMPEST_API auto current_path() -> path;
    TEMPEST_API void current_path(const path& p);

    [[nodiscard]] TEMPEST_API auto temp_directory_path() -> path;

    TEMPEST_API auto remove(const path& p) -> bool;

    [[nodiscard]] TEMPEST_API auto file_size(const path& p) -> size_t;

    class TEMPEST_API directory_entry
    {
      public:
        directory_entry() = default;
        explicit directory_entry(const path& p);
        directory_entry(path p, file_status status, file_status symlink_status, size_t file_size);

        [[nodiscard]] auto path() const noexcept -> const path&;
        operator const filesystem::path&() const noexcept;

        [[nodiscard]] auto exists() const -> bool;
        [[nodiscard]] auto is_block_file() const -> bool;
        [[nodiscard]] auto is_character_file() const -> bool;
        [[nodiscard]] auto is_directory() const -> bool;
        [[nodiscard]] auto is_fifo() const -> bool;
        [[nodiscard]] auto is_other() const -> bool;
        [[nodiscard]] auto is_regular_file() const -> bool;
        [[nodiscard]] auto is_socket() const -> bool;
        [[nodiscard]] auto is_symlink() const -> bool;

        [[nodiscard]] auto status() const -> file_status;
        [[nodiscard]] auto symlink_status() const -> file_status;

        [[nodiscard]] auto file_size() const -> size_t;

      private:
        filesystem::path _path;
        file_status _status{};
        file_status _symlink_status{};
        size_t _file_size{static_cast<size_t>(-1)};
    };

    // Filesystem Iterators
    class TEMPEST_API directory_iterator
    {
      public:
        using value_type = directory_entry;
        using reference = const directory_entry&;
        using pointer = const directory_entry*;
        using difference_type = ptrdiff_t;

        directory_iterator() noexcept = default;
        explicit directory_iterator(const path& p);

        auto operator*() const -> const directory_entry&;
        auto operator->() const -> const directory_entry*;

        auto operator++() -> directory_iterator&;

      private:
        path _dir;
        uint32_t _index = 0;

        vector<directory_entry> _entries;

        friend auto operator==(const directory_iterator& lhs, const directory_iterator& rhs) noexcept -> bool;
    };

    inline auto operator==(const directory_iterator& lhs, const directory_iterator& rhs) noexcept -> bool
    {
        // If the index is out of range, the iterator is at the end.
        // Check if both are at the end.
        if (lhs._index >= lhs._entries.size() && rhs._index >= rhs._entries.size())
        {
            return true;
        }

        // If only one is at the end, they are not equal.
        if (lhs._index >= lhs._entries.size() || rhs._index >= rhs._entries.size())
        {
            return false;
        }

        // Both are not at the end, compare the path and index
        return lhs._dir == rhs._dir && lhs._index == rhs._index;
    }

    TEMPEST_API
    inline auto begin(directory_iterator it) noexcept -> directory_iterator
    {
        return it;
    }

    TEMPEST_API
    inline auto end([[maybe_unused]] const directory_iterator& it) noexcept -> directory_iterator
    {
        return {};
    }

    [[nodiscard]] TEMPEST_API auto relative(const path& p) -> path;
    [[nodiscard]] TEMPEST_API auto relative(const path& p, const path& base) -> path;

    TEMPEST_API
    inline void swap(path& lhs, path& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    TEMPEST_API
    inline auto begin(const path& p) noexcept -> path::iterator
    {
        return p.begin();
    }

    TEMPEST_API
    inline auto cbegin(const path& p) noexcept -> path::iterator
    {
        return p.cbegin();
    }

    TEMPEST_API
    inline auto end(const path& p) noexcept -> path::iterator
    {
        return p.end();
    }

    TEMPEST_API
    inline auto cend(const path& p) noexcept -> path::iterator
    {
        return p.cend();
    }
} // namespace tempest::filesystem

#endif // tempest_filesystem_hpp