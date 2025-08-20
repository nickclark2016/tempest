#ifndef tempest_filesystem_hpp
#define tempest_filesystem_hpp

#include <tempest/concepts.hpp>
#include <tempest/enum.hpp>
#include <tempest/iterator.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/type_traits.hpp>

namespace tempest::filesystem
{
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
        concept path_source_type = (is_specialization_v<T, basic_string> || is_specialization_v<T, basic_string_view> ||
                                    (iterable<T> && character_type<remove_const_t<decay_t<remove_all_extents_t<T>>>>));

        string convert_wide_to_narrow(tempest::wstring_view wide_str);
        wstring convert_narrow_to_wide(tempest::string_view narrow_str);

        template <typename T>
        auto convert_to_native(const T& p)
        {
            using char_type = remove_const_t<typename iterator_traits<decay_t<T>>::value_type>;

            if constexpr (is_same_v<char_type, native_path_char_type>)
            {
                return p; // Already in native format
            }
            else if constexpr (is_same_v<char_type, char>)
            {
                static_assert(is_same_v<native_path_char_type, wchar_t>, "native_path_char_type must be a wchar_t");
                // The type is not the native path type and is char, so it must be converted to wide string
                return convert_narrow_to_wide(tempest::string_view(p));
            }
            else if constexpr (is_same_v<char_type, wchar_t>)
            {
                static_assert(is_same_v<native_path_char_type, char>, "native_path_char_type must be a char");
                // The type is not the native path type and is wchar_t, so it must be converted to narrow string
                return convert_wide_to_narrow(tempest::wstring_view(p));
            }
            else
            {
                static_assert(false, "Unsupported character type for path conversion.");
            }
        }
    } // namespace detail

    class path
    {
      public:
        using value_type = detail::native_path_char_type;
        using string_type = tempest::basic_string<value_type>;
        static constexpr value_type preferred_separator = detail::native_path_separator;

        path() noexcept = default;

        path(const path& p) : _path{p._path}
        {
        }

        path(path&& p) noexcept : _path{tempest::move(p._path)}
        {
        }

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

        path& operator=(const path& rhs)
        {
            _path = rhs._path;
            return *this;
        }

        path& operator=(path&& rhs) noexcept
        {
            _path = tempest::move(rhs._path);
            return *this;
        }

        path& operator=(string_type&& rhs) noexcept
        {
            _path = tempest::move(rhs);
            return *this;
        }

        path& operator=(nullptr_t) = delete;

        template <detail::path_source_type T>
        path& operator=(const T& t)
        {
            return assign(t);
        }

        path& assign(const path& p);
        path& assign(path&& p) noexcept;
        path& assign(string_type&& p) noexcept;

        template <detail::path_source_type T>
        path& assign(const T& p)
        {
            _path = detail::convert_to_native(p);
            return *this;
        }

        template <detail::path_source_type T>
        path& append(const T& p)
        {
            return _append(path(p));
        }

        template <detail::path_source_type T>
        path& operator/=(const T& p)
        {
            return _append(path(p));
        }

        template <detail::path_source_type T>
        path& concat(const T& p)
        {
            auto native = detail::convert_to_native(p);
            _path.append(tempest::move(native));
            return *this;
        }

        template <detail::path_source_type T>
        path& operator+=(const T& p)
        {
            return concat(p);
        }

        template <character_type T>
        path& operator+=(T x)
        {
            return concat(tempest::basic_string_view<value_type>(&x, 1));
        }

        path& operator+=(const path& p);
        path& operator+=(const string_type& p);
        path& operator+=(basic_string_view<value_type> p);
        path& operator+=(const value_type* p);
        path& operator+=(value_type ch);

        void clear();
        path& make_preferred();
        path& remove_filename();
        path& replace_filename(const path& replacement);
        path& replace_extension(const path& replacement);
        void swap(path& other) noexcept;

        [[nodiscard]] const value_type* c_str() const noexcept;
        [[nodiscard]] const string_type& native() const noexcept;
        [[nodiscard]] operator string_type() const;

        [[nodiscard]] tempest::string string() const;
        [[nodiscard]] tempest::wstring wstring() const;
        [[nodiscard]] tempest::string generic_string() const;
        [[nodiscard]] tempest::wstring generic_wstring() const;

        path root_name() const;
        path root_directory() const;
        path root_path() const;
        path relative_path() const;
        path parent_path() const;
        path filename() const;
        path stem() const;
        path extension() const;

        bool empty() const;
        bool has_root_path() const;
        bool has_root_name() const;
        bool has_root_directory() const;
        bool has_relative_path() const;
        bool has_parent_path() const;
        bool has_filename() const;
        bool has_stem() const;
        bool has_extension() const;
        bool is_absolute() const;
        bool is_relative() const;

      private:
        string_type _path;

        path& _append(const path& p);

        friend path operator/(const path& lhs, const path& rhs) noexcept;
    };

    bool operator==(const path& lhs, const path& rhs) noexcept;
    bool operator!=(const path& lhs, const path& rhs) noexcept;
    bool operator<(const path& lhs, const path& rhs) noexcept;
    bool operator<=(const path& lhs, const path& rhs) noexcept;
    bool operator>(const path& lhs, const path& rhs) noexcept;
    bool operator>=(const path& lhs, const path& rhs) noexcept;

    inline path operator/(const path& lhs, const path& rhs) noexcept
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

    class file_status
    {
      public:
        file_status() noexcept;
        explicit file_status(file_type type, permissions perms = permissions::unknown) noexcept;

        file_status(const file_status& other) noexcept = default;
        file_status(file_status&& other) noexcept = default;

        ~file_status() noexcept = default;

        file_status& operator=(const file_status& other) noexcept = default;
        file_status& operator=(file_status&& other) noexcept = default;

        file_type type() const noexcept
        {
            return _type;
        }

        void type(file_type t) noexcept
        {
            _type = t;
        }

        permissions perms() const noexcept
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

    [[nodiscard]] bool is_block_file(const file_status& status);
    [[nodiscard]] bool is_block_file(const path& p);
    [[nodiscard]] bool is_character_file(const file_status& status);
    [[nodiscard]] bool is_character_file(const path& p);
    [[nodiscard]] bool is_directory(const file_status& status);
    [[nodiscard]] bool is_directory(const path& p);
    [[nodiscard]] bool is_empty(const path& p);
    [[nodiscard]] bool is_fifo(const file_status& status);
    [[nodiscard]] bool is_fifo(const path& p);
    [[nodiscard]] bool is_other(const file_status& status);
    [[nodiscard]] bool is_other(const path& p);
    [[nodiscard]] bool is_regular_file(const file_status& status);
    [[nodiscard]] bool is_regular_file(const path& p);
    [[nodiscard]] bool is_socket(const file_status& status);
    [[nodiscard]] bool is_socket(const path& p);
    [[nodiscard]] bool is_symlink(const file_status& status);
    [[nodiscard]] bool is_symlink(const path& p);
    [[nodiscard]] bool status_known(const file_status& status);

    [[nodiscard]] file_status status(const path& p);
    [[nodiscard]] file_status symlink_status(const path& p);

    [[nodiscard]] bool exists(const file_status& status);
    [[nodiscard]] bool exists(const path& p);

    class directory_entry
    {
      public:
      private:
    };

    class directory_iterator
    {
      public:
        using value_type = directory_entry;
        using difference_type = std::ptrdiff_t;
        using pointer = const directory_entry*;
        using reference = const directory_entry&;

        directory_iterator() noexcept;
        explicit directory_iterator(const path& p);
        directory_iterator(const path& p, enum_mask<directory_options> opts) noexcept;

        directory_iterator(const directory_iterator& other) noexcept = default;
        directory_iterator(directory_iterator&& other) noexcept = default;

        ~directory_iterator() noexcept = default;

        directory_iterator& operator=(const directory_iterator& other) noexcept = default;
        directory_iterator& operator=(directory_iterator&& other) noexcept = default;

        directory_iterator operator++();

        pointer operator->() const noexcept;
        reference operator*() const noexcept;

      private:
        directory_entry _cur;
        enum_mask<directory_options> _opts;
    };
} // namespace tempest::filesystem

#endif // tempest_filesystem_hpp