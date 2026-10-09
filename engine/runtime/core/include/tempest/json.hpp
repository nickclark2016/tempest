#ifndef tempest_core_json_hpp
#define tempest_core_json_hpp

#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest
{
    enum class json_error : uint8_t
    {
        none = 0,
        type_mismatch,
        out_of_range,
        parse_error,
    };

    enum class json_format : uint8_t
    {
        compact,
        pretty,
    };

    class json_value;
    class json_object;
    class json_array;
    class json_object_iterator;
    class json_array_iterator;
    class json_document;
    class json_object_mut;
    class json_array_mut;
    class json_writer;

    class TEMPEST_API json_value
    {
      public:
        constexpr json_value() noexcept = default;
        constexpr explicit json_value(const void* val) noexcept : _val{val}
        {
        }
        json_value(const json_object& obj) noexcept;
        json_value(const json_array& arr) noexcept;

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

      private:
        friend class json_object;
        friend class json_array;
        friend class json_document;

        const void* _val{nullptr};
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
        json_object_iterator(const void* cur, size_t idx, size_t max) noexcept;

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
        const void* _cur{nullptr};
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
        json_array_iterator(const void* cur, size_t idx, size_t max) noexcept;

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
        const void* _cur{nullptr};
        size_t _idx{0};
        size_t _max{0};
    };

    class TEMPEST_API json_object
    {
      public:
        constexpr json_object() noexcept = default;
        constexpr explicit json_object(const void* val) noexcept : _val{val}
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

      private:
        friend class json_value;

        const void* _val{nullptr};
    };

    class TEMPEST_API json_array
    {
      public:
        constexpr json_array() noexcept = default;
        constexpr explicit json_array(const void* val) noexcept : _val{val}
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

      private:
        friend class json_value;

        const void* _val{nullptr};
    };

    inline json_value::json_value(const json_object& obj) noexcept : _val{obj._val}
    {
    }

    inline json_value::json_value(const json_array& arr) noexcept : _val{arr._val}
    {
    }

    class TEMPEST_API json_document
    {
      public:
        json_document() noexcept = default;
        explicit json_document(void* doc) noexcept : _doc{doc}
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

      private:
        void* _doc{nullptr};
    };

    class TEMPEST_API json_object_mut
    {
      public:
        constexpr json_object_mut() noexcept = default;
        constexpr json_object_mut(void* doc, void* val) noexcept : _doc{doc}, _val{val}
        {
        }

        [[nodiscard]] auto is_valid() const noexcept -> bool
        {
            return _val != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return is_valid();
        }

        void set(string_view key, bool val);
        void set(string_view key, int32_t val);
        void set(string_view key, uint32_t val);
        void set(string_view key, int64_t val);
        void set(string_view key, uint64_t val);
        void set(string_view key, float val);
        void set(string_view key, double val);
        void set(string_view key, string_view val);
        void set(string_view key, const char* val)
        {
            set(key, string_view{val});
        }
        void set(string_view key, const string& val)
        {
            set(key, string_view{val.data(), val.size()});
        }
        void set(string_view key, json_object_mut val);
        void set(string_view key, json_array_mut val);
        void set_null(string_view key);
        auto create_child_object(string_view key) -> json_object_mut;
        auto create_child_array(string_view key) -> json_array_mut;

      private:
        friend class json_writer;
        friend class json_array_mut;

        void* _doc{nullptr};
        void* _val{nullptr};
    };

    class TEMPEST_API json_array_mut
    {
      public:
        constexpr json_array_mut() noexcept = default;
        constexpr json_array_mut(void* doc, void* val) noexcept : _doc{doc}, _val{val}
        {
        }

        [[nodiscard]] auto is_valid() const noexcept -> bool
        {
            return _val != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return is_valid();
        }

        void push_back(bool val);
        void push_back(int32_t val);
        void push_back(uint32_t val);
        void push_back(int64_t val);
        void push_back(uint64_t val);
        void push_back(float val);
        void push_back(double val);
        void push_back(string_view val);
        void push_back(const char* val)
        {
            push_back(string_view{val});
        }
        void push_back(const string& val)
        {
            push_back(string_view{val.data(), val.size()});
        }
        void push_back(json_object_mut val);
        void push_back(json_array_mut val);
        void push_back_null();
        auto create_child_object() -> json_object_mut;
        auto create_child_array() -> json_array_mut;

      private:
        friend class json_writer;
        friend class json_object_mut;

        void* _doc{nullptr};
        void* _val{nullptr};
    };

    class TEMPEST_API json_writer
    {
      public:
        struct impl;

        json_writer();
        explicit json_writer(abstract_allocator& alloc);
        ~json_writer();

        json_writer(const json_writer&) = delete;
        auto operator=(const json_writer&) -> json_writer& = delete;

        json_writer(json_writer&& other) noexcept;
        auto operator=(json_writer&& other) noexcept -> json_writer&;

        auto create_object() -> json_object_mut;
        auto create_array() -> json_array_mut;
        void set_root(json_object_mut obj);
        void set_root(json_array_mut arr);

        [[nodiscard]] auto to_string(json_format fmt = json_format::compact) const -> string;
        [[nodiscard]] auto to_bytes(json_format fmt = json_format::compact) const -> vector<byte>;

      private:
        unique_ptr<impl> _impl;
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

    namespace core
    {
        using json_error = tempest::json_error;
        using json_format = tempest::json_format;
        using json_value = tempest::json_value;
        using json_member = tempest::json_member;
        using json_object_iterator = tempest::json_object_iterator;
        using json_array_iterator = tempest::json_array_iterator;
        using json_object = tempest::json_object;
        using json_array = tempest::json_array;
        using json_document = tempest::json_document;
        using json_object_mut = tempest::json_object_mut;
        using json_array_mut = tempest::json_array_mut;
        using json_writer = tempest::json_writer;
    } // namespace core
} // namespace tempest

#endif // tempest_core_json_hpp
