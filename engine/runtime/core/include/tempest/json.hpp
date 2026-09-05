#ifndef tempest_core_json_hpp
#define tempest_core_json_hpp

#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>

struct yyjson_doc;
struct yyjson_val;

namespace tempest
{
    enum class json_error : uint8_t
    {
        none = 0,
        type_mismatch,
        out_of_range,
        parse_error,
    };

    class json_value;
    class json_object;
    class json_array;
    class json_object_iterator;
    class json_array_iterator;
    class json_document;

    class TEMPEST_API json_value
    {
      public:
        constexpr json_value() noexcept = default;
        constexpr explicit json_value(const yyjson_val* val) noexcept : _val{val}
        {
        }
        constexpr json_value(const json_object& obj) noexcept;
        constexpr json_value(const json_array& arr) noexcept;

        [[nodiscard]] auto is_valid() const noexcept -> bool
        {
            return _val != nullptr;
        }

        [[nodiscard]] auto is_null() const noexcept -> bool;
        [[nodiscard]] auto is_bool() const noexcept -> bool;
        [[nodiscard]] auto is_number() const noexcept -> bool;
        [[nodiscard]] auto is_int() const noexcept -> bool;
        [[nodiscard]] auto is_uint() const noexcept -> bool;
        [[nodiscard]] auto is_real() const noexcept -> bool;
        [[nodiscard]] auto is_string() const noexcept -> bool;
        [[nodiscard]] auto is_array() const noexcept -> bool;
        [[nodiscard]] auto is_object() const noexcept -> bool;

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return is_valid() && !is_null();
        }

        [[nodiscard]] auto as_bool() const noexcept -> expected<bool, json_error>;
        [[nodiscard]] auto as_int64() const noexcept -> expected<int64_t, json_error>;
        [[nodiscard]] auto as_uint64() const noexcept -> expected<uint64_t, json_error>;
        [[nodiscard]] auto as_int32() const noexcept -> expected<int32_t, json_error>;
        [[nodiscard]] auto as_uint32() const noexcept -> expected<uint32_t, json_error>;
        [[nodiscard]] auto as_int16() const noexcept -> expected<int16_t, json_error>;
        [[nodiscard]] auto as_uint16() const noexcept -> expected<uint16_t, json_error>;
        [[nodiscard]] auto as_int8() const noexcept -> expected<int8_t, json_error>;
        [[nodiscard]] auto as_uint8() const noexcept -> expected<uint8_t, json_error>;
        [[nodiscard]] auto as_double() const noexcept -> expected<double, json_error>;
        [[nodiscard]] auto as_float() const noexcept -> expected<float, json_error>;
        [[nodiscard]] auto as_number() const noexcept -> expected<double, json_error>;
        [[nodiscard]] auto as_string() const noexcept -> expected<string_view, json_error>;
        [[nodiscard]] auto as_object() const noexcept -> expected<json_object, json_error>;
        [[nodiscard]] auto as_array() const noexcept -> expected<json_array, json_error>;

        template <typename T>
        [[nodiscard]] auto as() const noexcept -> expected<T, json_error>;

        template <typename T>
        auto get(T& out) const noexcept -> bool
        {
            auto res = as<T>();
            if (res.has_value())
            {
                out = *res;
                return true;
            }
            return false;
        }

        [[nodiscard]] auto operator[](string_view key) const noexcept -> json_value;
        [[nodiscard]] auto operator[](size_t index) const noexcept -> json_value;

        [[nodiscard]] auto size() const noexcept -> size_t;
        [[nodiscard]] auto empty() const noexcept -> bool;
        [[nodiscard]] auto contains(string_view key) const noexcept -> bool;

        [[nodiscard]] auto raw() const noexcept -> const yyjson_val*
        {
            return _val;
        }

      private:
        const yyjson_val* _val{nullptr};
    };

    struct json_member
    {
        string_view key;
        json_value value;
    };

    class TEMPEST_API json_object_iterator
    {
      public:
        using value_type = json_member;
        using difference_type = ptrdiff_t;
        using pointer = const json_member*;
        using reference = json_member;

        constexpr json_object_iterator() noexcept = default;
        json_object_iterator(const yyjson_val* cur, size_t idx, size_t max) noexcept;

        auto operator*() const noexcept -> json_member;
        auto operator++() noexcept -> json_object_iterator&;
        auto operator++(int) noexcept -> json_object_iterator;

        auto operator==(const json_object_iterator& rhs) const noexcept -> bool
        {
            return _idx == rhs._idx;
        }

        auto operator!=(const json_object_iterator& rhs) const noexcept -> bool
        {
            return _idx != rhs._idx;
        }

      private:
        const yyjson_val* _cur{nullptr};
        size_t _idx{0};
        size_t _max{0};
    };

    class TEMPEST_API json_array_iterator
    {
      public:
        using value_type = json_value;
        using difference_type = ptrdiff_t;
        using pointer = const json_value*;
        using reference = json_value;

        constexpr json_array_iterator() noexcept = default;
        json_array_iterator(const yyjson_val* cur, size_t idx, size_t max) noexcept;

        auto operator*() const noexcept -> json_value;
        auto operator++() noexcept -> json_array_iterator&;
        auto operator++(int) noexcept -> json_array_iterator;

        auto operator==(const json_array_iterator& rhs) const noexcept -> bool
        {
            return _idx == rhs._idx;
        }

        auto operator!=(const json_array_iterator& rhs) const noexcept -> bool
        {
            return _idx != rhs._idx;
        }

      private:
        const yyjson_val* _cur{nullptr};
        size_t _idx{0};
        size_t _max{0};
    };

    class TEMPEST_API json_object
    {
      public:
        constexpr json_object() noexcept = default;
        constexpr explicit json_object(const yyjson_val* val) noexcept : _val{val}
        {
        }

        static auto from_value(json_value val) noexcept -> expected<json_object, json_error>;

        [[nodiscard]] auto is_valid() const noexcept -> bool
        {
            return _val != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return is_valid();
        }

        [[nodiscard]] auto size() const noexcept -> size_t;
        [[nodiscard]] auto empty() const noexcept -> bool;
        [[nodiscard]] auto contains(string_view key) const noexcept -> bool;
        [[nodiscard]] auto operator[](string_view key) const noexcept -> json_value;

        [[nodiscard]] auto begin() const noexcept -> json_object_iterator;
        [[nodiscard]] auto end() const noexcept -> json_object_iterator;
        [[nodiscard]] auto cbegin() const noexcept -> json_object_iterator
        {
            return begin();
        }
        [[nodiscard]] auto cend() const noexcept -> json_object_iterator
        {
            return end();
        }

        [[nodiscard]] auto raw() const noexcept -> const yyjson_val*
        {
            return _val;
        }

      private:
        const yyjson_val* _val{nullptr};
    };

    class TEMPEST_API json_array
    {
      public:
        constexpr json_array() noexcept = default;
        constexpr explicit json_array(const yyjson_val* val) noexcept : _val{val}
        {
        }

        static auto from_value(json_value val) noexcept -> expected<json_array, json_error>;

        [[nodiscard]] auto is_valid() const noexcept -> bool
        {
            return _val != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return is_valid();
        }

        [[nodiscard]] auto size() const noexcept -> size_t;
        [[nodiscard]] auto empty() const noexcept -> bool;
        [[nodiscard]] auto operator[](size_t index) const noexcept -> json_value;

        [[nodiscard]] auto begin() const noexcept -> json_array_iterator;
        [[nodiscard]] auto end() const noexcept -> json_array_iterator;
        [[nodiscard]] auto cbegin() const noexcept -> json_array_iterator
        {
            return begin();
        }
        [[nodiscard]] auto cend() const noexcept -> json_array_iterator
        {
            return end();
        }

        [[nodiscard]] auto raw() const noexcept -> const yyjson_val*
        {
            return _val;
        }

      private:
        const yyjson_val* _val{nullptr};
    };

    constexpr json_value::json_value(const json_object& obj) noexcept : _val{obj.raw()}
    {
    }

    constexpr json_value::json_value(const json_array& arr) noexcept : _val{arr.raw()}
    {
    }

    class TEMPEST_API json_document
    {
      public:
        json_document() noexcept = default;
        explicit json_document(yyjson_doc* doc) noexcept : _doc{doc}
        {
        }
        ~json_document();

        json_document(const json_document&) = delete;
        auto operator=(const json_document&) -> json_document& = delete;

        json_document(json_document&& other) noexcept;
        auto operator=(json_document&& other) noexcept -> json_document&;

        static auto from_string(string_view json_str, abstract_allocator& alloc) -> expected<json_document, json_error>;
        static auto from_bytes(span<const byte> json_bytes, abstract_allocator& alloc)
            -> expected<json_document, json_error>;

        [[nodiscard]] auto root() const noexcept -> json_value;

        [[nodiscard]] auto is_valid() const noexcept -> bool
        {
            return _doc != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return is_valid();
        }

        [[nodiscard]] auto raw() const noexcept -> const yyjson_doc*
        {
            return _doc;
        }

      private:
        yyjson_doc* _doc{nullptr};
    };

    // Template specializations for json_value::as<T>()
    template <>
    inline auto json_value::as<bool>() const noexcept -> expected<bool, json_error>
    {
        return as_bool();
    }

    template <>
    inline auto json_value::as<int64_t>() const noexcept -> expected<int64_t, json_error>
    {
        return as_int64();
    }

    template <>
    inline auto json_value::as<uint64_t>() const noexcept -> expected<uint64_t, json_error>
    {
        return as_uint64();
    }

    template <>
    inline auto json_value::as<int32_t>() const noexcept -> expected<int32_t, json_error>
    {
        return as_int32();
    }

    template <>
    inline auto json_value::as<uint32_t>() const noexcept -> expected<uint32_t, json_error>
    {
        return as_uint32();
    }

    template <>
    inline auto json_value::as<int16_t>() const noexcept -> expected<int16_t, json_error>
    {
        return as_int16();
    }

    template <>
    inline auto json_value::as<uint16_t>() const noexcept -> expected<uint16_t, json_error>
    {
        return as_uint16();
    }

    template <>
    inline auto json_value::as<int8_t>() const noexcept -> expected<int8_t, json_error>
    {
        return as_int8();
    }

    template <>
    inline auto json_value::as<uint8_t>() const noexcept -> expected<uint8_t, json_error>
    {
        return as_uint8();
    }

    template <>
    inline auto json_value::as<double>() const noexcept -> expected<double, json_error>
    {
        return as_double();
    }

    template <>
    inline auto json_value::as<float>() const noexcept -> expected<float, json_error>
    {
        return as_float();
    }

    template <>
    inline auto json_value::as<string_view>() const noexcept -> expected<string_view, json_error>
    {
        return as_string();
    }

    template <>
    inline auto json_value::as<json_object>() const noexcept -> expected<json_object, json_error>
    {
        return as_object();
    }

    template <>
    inline auto json_value::as<json_array>() const noexcept -> expected<json_array, json_error>
    {
        return as_array();
    }
} // namespace tempest

#endif // tempest_core_json_hpp
