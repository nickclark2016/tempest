#include <tempest/json.hpp>
#include <tempest/limits.hpp>
#include <tempest/utility.hpp>

#include <yyjson.h>

namespace tempest
{
    namespace
    {
        constexpr auto json_default_alignment = size_t{16};

        auto yyjson_malloc_cb(void* ctx, size_t size) -> void*
        {
            auto* alloc = static_cast<abstract_allocator*>(ctx);
            return alloc->allocate(size, json_default_alignment);
        }

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        auto yyjson_realloc_cb(void* ctx, void* ptr, size_t old_size, size_t size) -> void*
        {
            auto* alloc = static_cast<abstract_allocator*>(ctx);
            if (ptr == nullptr)
            {
                return alloc->allocate(size, json_default_alignment);
            }
            if (size == 0)
            {
                alloc->deallocate(ptr);
                return nullptr;
            }
            auto* new_ptr = alloc->allocate(size, json_default_alignment);
            if (new_ptr != nullptr)
            {
                const auto copy_bytes = (old_size < size) ? old_size : size;
                tempest::memcpy(new_ptr, ptr, copy_bytes);
                alloc->deallocate(ptr);
            }
            return new_ptr;
        }

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        void yyjson_free_cb(void* ctx, void* ptr)
        {
            auto* alloc = static_cast<abstract_allocator*>(ctx);
            if (ptr != nullptr)
            {
                alloc->deallocate(ptr);
            }
        }

        inline auto cast_val(const void* val) noexcept -> yyjson_val*
        {
            return const_cast<yyjson_val*>(static_cast<const yyjson_val*>(val));
        }

        inline auto cast_doc(void* doc) noexcept -> yyjson_doc*
        {
            return static_cast<yyjson_doc*>(doc);
        }

        inline auto to_mut_doc(void* doc) noexcept -> yyjson_mut_doc*
        {
            return static_cast<yyjson_mut_doc*>(doc);
        }

        inline auto to_mut_val(void* val) noexcept -> yyjson_mut_val*
        {
            return static_cast<yyjson_mut_val*>(val);
        }
    } // namespace

    //==============================================================================
    // json_object_iterator
    //==============================================================================

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    json_object_iterator::json_object_iterator(const void* cur, size_t idx, size_t max) noexcept
        : _cur{cur}, _idx{idx}, _max{max}
    {
    }

    auto json_object_iterator::operator*() const noexcept -> json_member
    {
        auto* cur_val = cast_val(_cur);
        return json_member{
            .key = string_view{unsafe_yyjson_get_str(cur_val), unsafe_yyjson_get_len(cur_val)},
            .value = json_value{cur_val + 1},
        };
    }

    auto json_object_iterator::operator++() noexcept -> json_object_iterator&
    {
        if ((_cur != nullptr) && _idx < _max)
        {
            _cur = unsafe_yyjson_get_next(cast_val(_cur) + 1);
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

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    json_array_iterator::json_array_iterator(const void* cur, size_t idx, size_t max) noexcept
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
            _cur = unsafe_yyjson_get_next(cast_val(_cur));
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
        return _val == nullptr || yyjson_is_null(cast_val(_val));
    }

    auto json_value::is_bool() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_bool(cast_val(_val));
    }

    auto json_value::is_number() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_num(cast_val(_val));
    }

    auto json_value::is_int() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_int(cast_val(_val));
    }

    auto json_value::is_uint() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_uint(cast_val(_val));
    }

    auto json_value::is_real() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_real(cast_val(_val));
    }

    auto json_value::is_string() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_str(cast_val(_val));
    }

    auto json_value::is_array() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_arr(cast_val(_val));
    }

    auto json_value::is_object() const noexcept -> bool
    {
        return _val != nullptr && yyjson_is_obj(cast_val(_val));
    }

    auto json_value::as_bool() const noexcept -> expected<bool, json_error>
    {
        if ((_val != nullptr) && yyjson_is_bool(cast_val(_val)))
        {
            return yyjson_get_bool(cast_val(_val));
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_int64() const noexcept -> expected<int64_t, json_error>
    {
        if (_val == nullptr)
        {
            return unexpected(json_error::type_mismatch);
        }
        if (yyjson_is_sint(cast_val(_val)))
        {
            return yyjson_get_sint(cast_val(_val));
        }
        if (yyjson_is_uint(cast_val(_val)))
        {
            const auto uint_val = yyjson_get_uint(cast_val(_val));
            if (uint_val <= static_cast<uint64_t>(numeric_limits<int64_t>::max()))
            {
                return static_cast<int64_t>(uint_val);
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
        if (yyjson_is_uint(cast_val(_val)))
        {
            return yyjson_get_uint(cast_val(_val));
        }
        if (yyjson_is_sint(cast_val(_val)))
        {
            const auto sint_val = yyjson_get_sint(cast_val(_val));
            if (sint_val >= 0)
            {
                return static_cast<uint64_t>(sint_val);
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
        if ((_val != nullptr) && yyjson_is_real(cast_val(_val)))
        {
            return yyjson_get_real(cast_val(_val));
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_float() const noexcept -> expected<float, json_error>
    {
        if ((_val != nullptr) && yyjson_is_real(cast_val(_val)))
        {
            const auto double_val = yyjson_get_real(cast_val(_val));
            if (double_val > static_cast<double>(numeric_limits<float>::max()) ||
                double_val < static_cast<double>(numeric_limits<float>::lowest()))
            {
                return unexpected(json_error::out_of_range);
            }
            return static_cast<float>(double_val);
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_number() const noexcept -> expected<double, json_error>
    {
        if ((_val != nullptr) && yyjson_is_num(cast_val(_val)))
        {
            return yyjson_get_num(cast_val(_val));
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_string() const noexcept -> expected<string_view, json_error>
    {
        if ((_val != nullptr) && yyjson_is_str(cast_val(_val)))
        {
            return string_view{yyjson_get_str(cast_val(_val)), yyjson_get_len(cast_val(_val))};
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_object() const noexcept -> expected<json_object, json_error>
    {
        if ((_val != nullptr) && yyjson_is_obj(cast_val(_val)))
        {
            return json_object{_val};
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::as_array() const noexcept -> expected<json_array, json_error>
    {
        if ((_val != nullptr) && yyjson_is_arr(cast_val(_val)))
        {
            return json_array{_val};
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_value::operator[](string_view key) const noexcept -> json_value
    {
        if ((_val != nullptr) && yyjson_is_obj(cast_val(_val)))
        {
            return json_value{yyjson_obj_getn(cast_val(_val), key.data(), key.size())};
        }
        return json_value{nullptr};
    }

    auto json_value::operator[](size_t index) const noexcept -> json_value
    {
        if ((_val != nullptr) && yyjson_is_arr(cast_val(_val)))
        {
            return json_value{yyjson_arr_get(cast_val(_val), index)};
        }
        return json_value{nullptr};
    }

    auto json_value::size() const noexcept -> size_t
    {
        if (_val == nullptr)
        {
            return 0;
        }
        if (yyjson_is_arr(cast_val(_val)))
        {
            return yyjson_arr_size(cast_val(_val));
        }
        if (yyjson_is_obj(cast_val(_val)))
        {
            return yyjson_obj_size(cast_val(_val));
        }
        return 0;
    }

    auto json_value::empty() const noexcept -> bool
    {
        return size() == 0;
    }

    auto json_value::contains(string_view key) const noexcept -> bool
    {
        if ((_val != nullptr) && yyjson_is_obj(cast_val(_val)))
        {
            return yyjson_obj_getn(cast_val(_val), key.data(), key.size()) != nullptr;
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
            return json_object{val._val};
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_object::size() const noexcept -> size_t
    {
        return (_val != nullptr) ? yyjson_obj_size(cast_val(_val)) : 0;
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
        return yyjson_obj_getn(cast_val(_val), key.data(), key.size()) != nullptr;
    }

    auto json_object::operator[](string_view key) const noexcept -> json_value
    {
        if (_val != nullptr)
        {
            return json_value{yyjson_obj_getn(cast_val(_val), key.data(), key.size())};
        }
        return json_value{nullptr};
    }

    auto json_object::begin() const noexcept -> json_object_iterator
    {
        if ((_val != nullptr) && yyjson_is_obj(cast_val(_val)) && unsafe_yyjson_get_len(cast_val(_val)) > 0)
        {
            return json_object_iterator{unsafe_yyjson_get_first(cast_val(_val)), 0,
                                        unsafe_yyjson_get_len(cast_val(_val))};
        }
        return json_object_iterator{nullptr, 0, 0};
    }

    auto json_object::end() const noexcept -> json_object_iterator
    {
        if ((_val != nullptr) && yyjson_is_obj(cast_val(_val)))
        {
            const auto len = unsafe_yyjson_get_len(cast_val(_val));
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
            return json_array{val._val};
        }
        return unexpected(json_error::type_mismatch);
    }

    auto json_array::size() const noexcept -> size_t
    {
        return (_val != nullptr) ? yyjson_arr_size(cast_val(_val)) : 0;
    }

    auto json_array::empty() const noexcept -> bool
    {
        return size() == 0;
    }

    auto json_array::operator[](size_t index) const noexcept -> json_value
    {
        if (_val != nullptr)
        {
            return json_value{yyjson_arr_get(cast_val(_val), index)};
        }
        return json_value{nullptr};
    }

    auto json_array::begin() const noexcept -> json_array_iterator
    {
        if ((_val != nullptr) && yyjson_is_arr(cast_val(_val)) && unsafe_yyjson_get_len(cast_val(_val)) > 0)
        {
            return json_array_iterator{unsafe_yyjson_get_first(cast_val(_val)), 0,
                                       unsafe_yyjson_get_len(cast_val(_val))};
        }
        return json_array_iterator{nullptr, 0, 0};
    }

    auto json_array::end() const noexcept -> json_array_iterator
    {
        if ((_val != nullptr) && yyjson_is_arr(cast_val(_val)))
        {
            const auto len = unsafe_yyjson_get_len(cast_val(_val));
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
            yyjson_doc_free(cast_doc(_doc));
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
                yyjson_doc_free(cast_doc(_doc));
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
        if (doc == nullptr)
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
        if (doc == nullptr)
        {
            return unexpected(json_error::parse_error);
        }
        return json_document{doc};
    }

    auto json_document::root() const noexcept -> json_value
    {
        if (_doc != nullptr)
        {
            return json_value{yyjson_doc_get_root(cast_doc(_doc))};
        }
        return json_value{nullptr};
    }

    //==============================================================================
    // json_object_mut
    //==============================================================================

    void json_object_mut::set(string_view key, bool val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* obj = to_mut_val(_val);
        auto* k = yyjson_mut_strncpy(doc, key.data(), key.size());
        auto* v = yyjson_mut_bool(doc, val);
        yyjson_mut_obj_add(obj, k, v);
    }

    void json_object_mut::set(string_view key, int32_t val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* obj = to_mut_val(_val);
        auto* k = yyjson_mut_strncpy(doc, key.data(), key.size());
        auto* v = yyjson_mut_int(doc, val);
        yyjson_mut_obj_add(obj, k, v);
    }

    void json_object_mut::set(string_view key, uint32_t val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* obj = to_mut_val(_val);
        auto* k = yyjson_mut_strncpy(doc, key.data(), key.size());
        auto* v = yyjson_mut_uint(doc, val);
        yyjson_mut_obj_add(obj, k, v);
    }

    void json_object_mut::set(string_view key, int64_t val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* obj = to_mut_val(_val);
        auto* k = yyjson_mut_strncpy(doc, key.data(), key.size());
        auto* v = yyjson_mut_int(doc, val);
        yyjson_mut_obj_add(obj, k, v);
    }

    void json_object_mut::set(string_view key, uint64_t val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* obj = to_mut_val(_val);
        auto* k = yyjson_mut_strncpy(doc, key.data(), key.size());
        auto* v = yyjson_mut_uint(doc, val);
        yyjson_mut_obj_add(obj, k, v);
    }

    void json_object_mut::set(string_view key, float val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* obj = to_mut_val(_val);
        auto* k = yyjson_mut_strncpy(doc, key.data(), key.size());
        auto* v = yyjson_mut_float(doc, val);
        yyjson_mut_obj_add(obj, k, v);
    }

    void json_object_mut::set(string_view key, double val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* obj = to_mut_val(_val);
        auto* k = yyjson_mut_strncpy(doc, key.data(), key.size());
        auto* v = yyjson_mut_double(doc, val);
        yyjson_mut_obj_add(obj, k, v);
    }

    void json_object_mut::set(string_view key, string_view val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* obj = to_mut_val(_val);
        auto* k = yyjson_mut_strncpy(doc, key.data(), key.size());
        auto* v = yyjson_mut_strncpy(doc, val.data(), val.size());
        yyjson_mut_obj_add(obj, k, v);
    }

    void json_object_mut::set(string_view key, json_object_mut val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* obj = to_mut_val(_val);
        auto* k = yyjson_mut_strncpy(doc, key.data(), key.size());
        auto* v = to_mut_val(val._val);
        yyjson_mut_obj_add(obj, k, v);
    }

    void json_object_mut::set(string_view key, json_array_mut val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* obj = to_mut_val(_val);
        auto* k = yyjson_mut_strncpy(doc, key.data(), key.size());
        auto* v = to_mut_val(val._val);
        yyjson_mut_obj_add(obj, k, v);
    }

    void json_object_mut::set_null(string_view key)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* obj = to_mut_val(_val);
        auto* k = yyjson_mut_strncpy(doc, key.data(), key.size());
        auto* v = yyjson_mut_null(doc);
        yyjson_mut_obj_add(obj, k, v);
    }

    auto json_object_mut::create_child_object(string_view key) -> json_object_mut
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return json_object_mut{};
        }
        auto* doc = to_mut_doc(_doc);
        auto* obj = to_mut_val(_val);
        auto* k = yyjson_mut_strncpy(doc, key.data(), key.size());
        auto* child = yyjson_mut_obj(doc);
        yyjson_mut_obj_add(obj, k, child);
        return json_object_mut{_doc, child};
    }

    auto json_object_mut::create_child_array(string_view key) -> json_array_mut
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return json_array_mut{};
        }
        auto* doc = to_mut_doc(_doc);
        auto* obj = to_mut_val(_val);
        auto* k = yyjson_mut_strncpy(doc, key.data(), key.size());
        auto* child = yyjson_mut_arr(doc);
        yyjson_mut_obj_add(obj, k, child);
        return json_array_mut{_doc, child};
    }

    //==============================================================================
    // json_array_mut
    //==============================================================================

    void json_array_mut::push_back(bool val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* arr = to_mut_val(_val);
        auto* v = yyjson_mut_bool(doc, val);
        yyjson_mut_arr_append(arr, v);
    }

    void json_array_mut::push_back(int32_t val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* arr = to_mut_val(_val);
        auto* v = yyjson_mut_int(doc, val);
        yyjson_mut_arr_append(arr, v);
    }

    void json_array_mut::push_back(uint32_t val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* arr = to_mut_val(_val);
        auto* v = yyjson_mut_uint(doc, val);
        yyjson_mut_arr_append(arr, v);
    }

    void json_array_mut::push_back(int64_t val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* arr = to_mut_val(_val);
        auto* v = yyjson_mut_int(doc, val);
        yyjson_mut_arr_append(arr, v);
    }

    void json_array_mut::push_back(uint64_t val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* arr = to_mut_val(_val);
        auto* v = yyjson_mut_uint(doc, val);
        yyjson_mut_arr_append(arr, v);
    }

    void json_array_mut::push_back(float val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* arr = to_mut_val(_val);
        auto* v = yyjson_mut_float(doc, val);
        yyjson_mut_arr_append(arr, v);
    }

    void json_array_mut::push_back(double val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* arr = to_mut_val(_val);
        auto* v = yyjson_mut_double(doc, val);
        yyjson_mut_arr_append(arr, v);
    }

    void json_array_mut::push_back(string_view val)
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* arr = to_mut_val(_val);
        auto* v = yyjson_mut_strncpy(doc, val.data(), val.size());
        yyjson_mut_arr_append(arr, v);
    }

    void json_array_mut::push_back(json_object_mut val)
    {
        if (_val == nullptr)
        {
            return;
        }
        auto* arr = to_mut_val(_val);
        auto* v = to_mut_val(val._val);
        yyjson_mut_arr_append(arr, v);
    }

    void json_array_mut::push_back(json_array_mut val)
    {
        if (_val == nullptr)
        {
            return;
        }
        auto* arr = to_mut_val(_val);
        auto* v = to_mut_val(val._val);
        yyjson_mut_arr_append(arr, v);
    }

    void json_array_mut::push_back_null()
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return;
        }
        auto* doc = to_mut_doc(_doc);
        auto* arr = to_mut_val(_val);
        auto* v = yyjson_mut_null(doc);
        yyjson_mut_arr_append(arr, v);
    }

    auto json_array_mut::create_child_object() -> json_object_mut
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return json_object_mut{};
        }
        auto* doc = to_mut_doc(_doc);
        auto* arr = to_mut_val(_val);
        auto* child = yyjson_mut_obj(doc);
        yyjson_mut_arr_append(arr, child);
        return json_object_mut{_doc, child};
    }

    auto json_array_mut::create_child_array() -> json_array_mut
    {
        if (_doc == nullptr || _val == nullptr)
        {
            return json_array_mut{};
        }
        auto* doc = to_mut_doc(_doc);
        auto* arr = to_mut_val(_val);
        auto* child = yyjson_mut_arr(doc);
        yyjson_mut_arr_append(arr, child);
        return json_array_mut{_doc, child};
    }

    //==============================================================================
    // json_writer::impl
    //==============================================================================

    struct json_writer::impl
    {
        system_allocator default_alloc;
        abstract_allocator* alloc{nullptr};
        yyjson_alc alc{};
        yyjson_mut_doc* doc{nullptr};

        impl() : alloc{&default_alloc}
        {
            init_doc();
        }

        explicit impl(abstract_allocator& custom_alloc) : alloc{&custom_alloc}
        {
            init_doc();
        }

        ~impl()
        {
            if (doc != nullptr)
            {
                yyjson_mut_doc_free(doc);
                doc = nullptr;
            }
        }

        impl(const impl&) = delete;
        auto operator=(const impl&) -> impl& = delete;
        impl(impl&&) = delete;
        auto operator=(impl&&) -> impl& = delete;

        void init_doc()
        {
            alc = yyjson_alc{
                .malloc = yyjson_malloc_cb,
                .realloc = yyjson_realloc_cb,
                .free = yyjson_free_cb,
                .ctx = alloc,
            };
            doc = yyjson_mut_doc_new(&alc);
        }

        auto create_object() -> json_object_mut
        {
            auto* obj = yyjson_mut_obj(doc);
            return json_object_mut{doc, obj};
        }

        auto create_array() -> json_array_mut
        {
            auto* arr = yyjson_mut_arr(doc);
            return json_array_mut{doc, arr};
        }

        void set_root(json_object_mut obj)
        {
            yyjson_mut_doc_set_root(doc, to_mut_val(obj._val));
        }

        void set_root(json_array_mut arr)
        {
            yyjson_mut_doc_set_root(doc, to_mut_val(arr._val));
        }

        auto to_string(json_format fmt) const -> string
        {
            if (doc == nullptr)
            {
                return string{};
            }

            const auto flags = (fmt == json_format::pretty) ? yyjson_write_flag{YYJSON_WRITE_PRETTY_TWO_SPACES}
                                                            : yyjson_write_flag{YYJSON_WRITE_NOFLAG};

            auto len = size_t{0};
            auto err = yyjson_write_err{};
            auto* buf = yyjson_mut_write_opts(doc, flags, &alc, &len, &err);
            if (buf == nullptr)
            {
                return string{};
            }

            auto result = string{buf, len};
            alc.free(alc.ctx, buf);
            return result;
        }

        auto to_bytes(json_format fmt) const -> vector<byte>
        {
            if (doc == nullptr)
            {
                return vector<byte>{};
            }

            const auto flags = (fmt == json_format::pretty) ? yyjson_write_flag{YYJSON_WRITE_PRETTY_TWO_SPACES}
                                                            : yyjson_write_flag{YYJSON_WRITE_NOFLAG};

            auto len = size_t{0};
            auto err = yyjson_write_err{};
            auto* buf = yyjson_mut_write_opts(doc, flags, &alc, &len, &err);
            if (buf == nullptr)
            {
                return vector<byte>{};
            }

            auto result = vector<byte>{};
            result.resize(len);
            tempest::memcpy(result.data(), buf, len);
            alc.free(alc.ctx, buf);
            return result;
        }
    };

    //==============================================================================
    // json_writer
    //==============================================================================

    json_writer::json_writer() : _impl{make_unique<impl>()}
    {
    }

    json_writer::json_writer(abstract_allocator& alloc) : _impl{make_unique<impl>(alloc)}
    {
    }

    json_writer::~json_writer() = default;

    json_writer::json_writer(json_writer&& other) noexcept = default;

    auto json_writer::operator=(json_writer&& other) noexcept -> json_writer& = default;

    auto json_writer::create_object() -> json_object_mut
    {
        return _impl ? _impl->create_object() : json_object_mut{};
    }

    auto json_writer::create_array() -> json_array_mut
    {
        return _impl ? _impl->create_array() : json_array_mut{};
    }

    void json_writer::set_root(json_object_mut obj)
    {
        if (_impl)
        {
            _impl->set_root(obj);
        }
    }

    void json_writer::set_root(json_array_mut arr)
    {
        if (_impl)
        {
            _impl->set_root(arr);
        }
    }

    auto json_writer::to_string(json_format fmt) const -> string
    {
        return _impl ? _impl->to_string(fmt) : string{};
    }

    auto json_writer::to_bytes(json_format fmt) const -> vector<byte>
    {
        return _impl ? _impl->to_bytes(fmt) : vector<byte>{};
    }
} // namespace tempest
