#include <tempest/json.hpp>
#include <tempest/limits.hpp>
#include <tempest/utility.hpp>

#include <yyjson.h>

namespace tempest
{
    namespace
    {
        auto yyjson_malloc_cb(void* ctx, size_t size) -> void*
        {
            auto* alloc = static_cast<abstract_allocator*>(ctx);
            return alloc->allocate(size, 16);
        }

        auto yyjson_realloc_cb(void* ctx, void* ptr, size_t old_size, size_t size) -> void*
        {
            auto* alloc = static_cast<abstract_allocator*>(ctx);
            if (ptr == nullptr)
            {
                return alloc->allocate(size, 16);
            }
            if (size == 0)
            {
                alloc->deallocate(ptr);
                return nullptr;
            }
            auto* new_ptr = alloc->allocate(size, 16);
            if (new_ptr != nullptr)
            {
                const auto copy_bytes = (old_size < size) ? old_size : size;
                tempest::memcpy(new_ptr, ptr, copy_bytes);
                alloc->deallocate(ptr);
            }
            return new_ptr;
        }

        void yyjson_free_cb(void* ctx, void* ptr)
        {
            auto* alloc = static_cast<abstract_allocator*>(ctx);
            if (ptr != nullptr)
            {
                alloc->deallocate(ptr);
            }
        }

        inline auto unconst(const yyjson_val* val) noexcept -> yyjson_val*
        {
            return const_cast<yyjson_val*>(val);
        }
    } // namespace

    //==============================================================================
    // json_object_iterator
    //==============================================================================

    json_object_iterator::json_object_iterator(const yyjson_val* cur, size_t idx, size_t max) noexcept
        : _cur{cur}, _idx{idx}, _max{max}
    {
    }

    auto json_object_iterator::operator*() const noexcept -> json_member
    {
        return json_member{
            .key = string_view{unsafe_yyjson_get_str(unconst(_cur)), unsafe_yyjson_get_len(unconst(_cur))},
            .value = json_value{_cur + 1},
        };
    }

    auto json_object_iterator::operator++() noexcept -> json_object_iterator&
    {
        if ((_cur != nullptr) && _idx < _max)
        {
            _cur = unsafe_yyjson_get_next(unconst(_cur + 1));
            ++_idx;
        }
        return *this;
    }

    auto json_object_iterator::operator++(int) noexcept -> json_object_iterator
    {
        auto tmp = *this;
        ++(*this);
        return tmp;
    }

    //==============================================================================
    // json_array_iterator
    //==============================================================================

    json_array_iterator::json_array_iterator(const yyjson_val* cur, size_t idx, size_t max) noexcept
        : _cur{cur}, _idx{idx}, _max{max}
    {
    }

    auto json_array_iterator::operator*() const noexcept -> json_value
    {
        return json_value{_cur};
    }

    auto json_array_iterator::operator++() noexcept -> json_array_iterator&
    {
        if ((_cur != nullptr) && _idx < _max)
        {
            _cur = unsafe_yyjson_get_next(unconst(_cur));
            ++_idx;
        }
        return *this;
    }

    auto json_array_iterator::operator++(int) noexcept -> json_array_iterator
    {
        auto tmp = *this;
        ++(*this);
        return tmp;
    }

    //==============================================================================
    // json_value
    //==============================================================================

    auto json_value::is_null() const noexcept -> bool
    {
        return _val == nullptr || yyjson_is_null(unconst(_val));
    }

    auto json_value::is_bool() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_bool(unconst(_val));
    }

    auto json_value::is_number() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_num(unconst(_val));
    }

    auto json_value::is_int() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_int(unconst(_val));
    }

    auto json_value::is_uint() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_uint(unconst(_val));
    }

    auto json_value::is_real() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_real(unconst(_val));
    }

    auto json_value::is_string() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_str(unconst(_val));
    }

    auto json_value::is_array() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_arr(unconst(_val));
    }

    auto json_value::is_object() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_obj(unconst(_val));
    }

    auto json_value::as_bool() const noexcept -> expected<bool, json_error>
    {
        if (_val && yyjson_is_bool(unconst(_val)))
        {
            return yyjson_get_bool(unconst(_val));
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_int64() const noexcept -> expected<int64_t, json_error>
    {
        if (_val == nullptr)
        {
            return unexpected(json_error::type_mismatch);
        }
        if (yyjson_is_sint(unconst(_val)))
        {
            return yyjson_get_sint(unconst(_val));
        }
        if (yyjson_is_uint(unconst(_val)))
        {
            const auto u = yyjson_get_uint(unconst(_val));
            if (u <= static_cast<uint64_t>(numeric_limits<int64_t>::max()))
            {
                return static_cast<int64_t>(u);
            }
            return unexpected(json_error::out_of_range);
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_uint64() const noexcept -> expected<uint64_t, json_error>
    {
        if (_val == nullptr)
        {
            return unexpected(json_error::type_mismatch);
        }
        if (yyjson_is_uint(unconst(_val)))
        {
            return yyjson_get_uint(unconst(_val));
        }
        if (yyjson_is_sint(unconst(_val)))
        {
            const auto s = yyjson_get_sint(unconst(_val));
            if (s >= 0)
            {
                return static_cast<uint64_t>(s);
            }
            return unexpected(json_error::out_of_range);
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_int32() const noexcept -> expected<int32_t, json_error>
    {
        auto res = as_int64();
        if (!res)
        {
            return unexpected(res.error());
        }
        if (*res < numeric_limits<int32_t>::min() || *res > numeric_limits<int32_t>::max())
        {
            return unexpected(json_error::out_of_range);
        }
        return static_cast<int32_t>(*res);
    }

    auto json_value::as_uint32() const noexcept -> expected<uint32_t, json_error>
    {
        auto res = as_uint64();
        if (!res)
        {
            return unexpected(res.error());
        }
        if (*res > numeric_limits<uint32_t>::max())
        {
            return unexpected(json_error::out_of_range);
        }
        return static_cast<uint32_t>(*res);
    }

    auto json_value::as_int16() const noexcept -> expected<int16_t, json_error>
    {
        auto res = as_int64();
        if (!res)
        {
            return unexpected(res.error());
        }
        if (*res < numeric_limits<int16_t>::min() || *res > numeric_limits<int16_t>::max())
        {
            return unexpected(json_error::out_of_range);
        }
        return static_cast<int16_t>(*res);
    }

    auto json_value::as_uint16() const noexcept -> expected<uint16_t, json_error>
    {
        auto res = as_uint64();
        if (!res)
        {
            return unexpected(res.error());
        }
        if (*res > numeric_limits<uint16_t>::max())
        {
            return unexpected(json_error::out_of_range);
        }
        return static_cast<uint16_t>(*res);
    }

    auto json_value::as_int8() const noexcept -> expected<int8_t, json_error>
    {
        auto res = as_int64();
        if (!res)
        {
            return unexpected(res.error());
        }
        if (*res < numeric_limits<int8_t>::min() || *res > numeric_limits<int8_t>::max())
        {
            return unexpected(json_error::out_of_range);
        }
        return static_cast<int8_t>(*res);
    }

    auto json_value::as_uint8() const noexcept -> expected<uint8_t, json_error>
    {
        auto res = as_uint64();
        if (!res)
        {
            return unexpected(res.error());
        }
        if (*res > numeric_limits<uint8_t>::max())
        {
            return unexpected(json_error::out_of_range);
        }
        return static_cast<uint8_t>(*res);
    }

    auto json_value::as_double() const noexcept -> expected<double, json_error>
    {
        if (_val && yyjson_is_real(unconst(_val)))
        {
            return yyjson_get_real(unconst(_val));
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_float() const noexcept -> expected<float, json_error>
    {
        if (_val && yyjson_is_real(unconst(_val)))
        {
            const auto d = yyjson_get_real(unconst(_val));
            if (d > static_cast<double>(numeric_limits<float>::max()) ||
                d < static_cast<double>(numeric_limits<float>::lowest()))
            {
                return unexpected(json_error::out_of_range);
            }
            return static_cast<float>(d);
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_number() const noexcept -> expected<double, json_error>
    {
        if (_val && yyjson_is_num(unconst(_val)))
        {
            return yyjson_get_num(unconst(_val));
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_string() const noexcept -> expected<string_view, json_error>
    {
        if (_val && yyjson_is_str(unconst(_val)))
        {
            return string_view{yyjson_get_str(unconst(_val)), yyjson_get_len(unconst(_val))};
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_object() const noexcept -> expected<json_object, json_error>
    {
        if (_val && yyjson_is_obj(unconst(_val)))
        {
            return json_object{_val};
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_array() const noexcept -> expected<json_array, json_error>
    {
        if (_val && yyjson_is_arr(unconst(_val)))
        {
            return json_array{_val};
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::operator[](string_view key) const noexcept -> json_value
    {
        if (_val && yyjson_is_obj(unconst(_val)))
        {
            return json_value{yyjson_obj_getn(unconst(_val), key.data(), key.size())};
        }
        return json_value{nullptr};
    }

    auto json_value::operator[](size_t index) const noexcept -> json_value
    {
        if (_val && yyjson_is_arr(unconst(_val)))
        {
            return json_value{yyjson_arr_get(unconst(_val), index)};
        }
        return json_value{nullptr};
    }

    auto json_value::size() const noexcept -> size_t
    {
        if (_val == nullptr)
        {
            return 0;
        }
        if (yyjson_is_arr(unconst(_val)))
        {
            return yyjson_arr_size(unconst(_val));
        }
        if (yyjson_is_obj(unconst(_val)))
        {
            return yyjson_obj_size(unconst(_val));
        }
        return 0;
    }

    auto json_value::empty() const noexcept -> bool
    {
        return size() == 0;
    }

    auto json_value::contains(string_view key) const noexcept -> bool
    {
        if (_val && yyjson_is_obj(unconst(_val)))
        {
            return yyjson_obj_getn(unconst(_val), key.data(), key.size()) != nullptr;
        }
        return false;
    }

    //==============================================================================
    // json_object
    //==============================================================================

    auto json_object::from_value(json_value val) noexcept -> expected<json_object, json_error>
    {
        if (val.is_object())
        {
            return json_object{val.raw()};
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_object::size() const noexcept -> size_t
    {
        return (_val != nullptr) ? yyjson_obj_size(unconst(_val)) : 0;
    }

    auto json_object::empty() const noexcept -> bool
    {
        return size() == 0;
    }

    auto json_object::contains(string_view key) const noexcept -> bool
    {
        if (_val == nullptr)
        {
            return false;
        }
        return yyjson_obj_getn(unconst(_val), key.data(), key.size()) != nullptr;
    }

    auto json_object::operator[](string_view key) const noexcept -> json_value
    {
        if (_val != nullptr)
        {
            return json_value{yyjson_obj_getn(unconst(_val), key.data(), key.size())};
        }
        return json_value{nullptr};
    }

    auto json_object::begin() const noexcept -> json_object_iterator
    {
        if (_val && yyjson_is_obj(unconst(_val)) && unsafe_yyjson_get_len(unconst(_val)) > 0)
        {
            return json_object_iterator{unsafe_yyjson_get_first(unconst(_val)), 0,
                                        unsafe_yyjson_get_len(unconst(_val))};
        }
        return json_object_iterator{nullptr, 0, 0};
    }

    auto json_object::end() const noexcept -> json_object_iterator
    {
        if (_val && yyjson_is_obj(unconst(_val)))
        {
            const auto len = unsafe_yyjson_get_len(unconst(_val));
            return json_object_iterator{nullptr, len, len};
        }
        return json_object_iterator{nullptr, 0, 0};
    }

    //==============================================================================
    // json_array
    //==============================================================================

    auto json_array::from_value(json_value val) noexcept -> expected<json_array, json_error>
    {
        if (val.is_array())
        {
            return json_array{val.raw()};
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_array::size() const noexcept -> size_t
    {
        return (_val != nullptr) ? yyjson_arr_size(unconst(_val)) : 0;
    }

    auto json_array::empty() const noexcept -> bool
    {
        return size() == 0;
    }

    auto json_array::operator[](size_t index) const noexcept -> json_value
    {
        if (_val != nullptr)
        {
            return json_value{yyjson_arr_get(unconst(_val), index)};
        }
        return json_value{nullptr};
    }

    auto json_array::begin() const noexcept -> json_array_iterator
    {
        if (_val && yyjson_is_arr(unconst(_val)) && unsafe_yyjson_get_len(unconst(_val)) > 0)
        {
            return json_array_iterator{unsafe_yyjson_get_first(unconst(_val)), 0, unsafe_yyjson_get_len(unconst(_val))};
        }
        return json_array_iterator{nullptr, 0, 0};
    }

    auto json_array::end() const noexcept -> json_array_iterator
    {
        if (_val && yyjson_is_arr(unconst(_val)))
        {
            const auto len = unsafe_yyjson_get_len(unconst(_val));
            return json_array_iterator{nullptr, len, len};
        }
        return json_array_iterator{nullptr, 0, 0};
    }

    //==============================================================================
    // json_document
    //==============================================================================

    json_document::~json_document()
    {
        if (_doc != nullptr)
        {
            yyjson_doc_free(_doc);
            _doc = nullptr;
        }
    }

    json_document::json_document(json_document&& other) noexcept : _doc{other._doc}
    {
        other._doc = nullptr;
    }

    auto json_document::operator=(json_document&& other) noexcept -> json_document&
    {
        if (this != &other)
        {
            if (_doc != nullptr)
            {
                yyjson_doc_free(_doc);
            }
            _doc = other._doc;
            other._doc = nullptr;
        }
        return *this;
    }

    auto json_document::from_string(string_view json_str, abstract_allocator& alloc)
        -> expected<json_document, json_error>
    {
        auto alc = yyjson_alc{
            .malloc = yyjson_malloc_cb,
            .realloc = yyjson_realloc_cb,
            .free = yyjson_free_cb,
            .ctx = &alloc,
        };

        auto err = yyjson_read_err{};
        auto* doc = yyjson_read_opts(const_cast<char*>(json_str.data()), json_str.size(), 0, &alc, &err);
        if (!doc)
        {
            return unexpected(json_error::parse_error);
        }
        return json_document{doc};
    }

    auto json_document::from_bytes(span<const byte> json_bytes, abstract_allocator& alloc)
        -> expected<json_document, json_error>
    {
        auto alc = yyjson_alc{
            .malloc = yyjson_malloc_cb,
            .realloc = yyjson_realloc_cb,
            .free = yyjson_free_cb,
            .ctx = &alloc,
        };

        auto err = yyjson_read_err{};
        auto* doc = yyjson_read_opts(const_cast<char*>(reinterpret_cast<const char*>(json_bytes.data())),
                                     json_bytes.size(), 0, &alc, &err);
        if (!doc)
        {
            return unexpected(json_error::parse_error);
        }
        return json_document{doc};
    }

    auto json_document::root() const noexcept -> json_value
    {
        if (_doc != nullptr)
        {
            return json_value{yyjson_doc_get_root(_doc)};
        }
        return json_value{nullptr};
    }
} // namespace tempest
