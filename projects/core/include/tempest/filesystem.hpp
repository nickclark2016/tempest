#ifndef tempest_filesystem_hpp
#define tempest_filesystem_hpp

#include <tempest/concepts.hpp>
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
                                    character_type<remove_const_t<typename iterator_traits<decay_t<T>>::value_type>>);

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
} // namespace tempest::filesystem

#endif // tempest_filesystem_hpp