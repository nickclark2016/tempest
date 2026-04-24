#ifndef tempest_serialization_serial_hpp
#define tempest_serialization_serial_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/bit.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::serialization
{
    struct TEMPEST_API binary_header
    {
        array<uint8_t, 4> magic; // "TEBF" (tempest engine binary format)
        uint16_t version;
        uint16_t flags;
        uint64_t data_length;
    };

    template <typename T>
    concept archive = requires(T arch) {
        { arch.write(span<const byte>{}) } -> same_as<void>;
        { arch.read(size_t{}) } -> same_as<span<const byte>>;
    };

    class TEMPEST_API binary_archive
    {
      public:
        auto write(span<const byte> data) -> void;
        auto write(vector<byte> data) -> void;
        [[nodiscard]] auto read(size_t count) -> span<const byte>;
        [[nodiscard]] auto written_size() const noexcept -> size_t;

      private:
        vector<byte> _buffer;
        size_t _read_offset{};
    };

    template <archive A, typename T>
    struct serializer
    {
        static auto serialize(A& archive, const T& value) -> void = delete;
        static auto deserialize(A& archive) -> T = delete;
    };

    template <typename T>
        requires is_trivially_copyable_v<T>
    struct serializer<binary_archive, T>
    {
        TEMPEST_API
        static auto serialize(binary_archive& archive, const T& value) -> void
        {
            const auto obj_span = span<const T>{&value, 1};
            const auto byte_span = as_bytes(obj_span);
            archive.write(byte_span);
        }

        TEMPEST_API
        static auto deserialize(binary_archive& archive) -> T
        {
            const auto byte_span = archive.read(sizeof(T));
            // Copy instead of start_lifetime_as, since the data in the archive may not be properly aligned for T
            T value;
            tempest::memcpy(&value, byte_span.data(), sizeof(T));
            return value;
        }
    };

    template <typename T, typename Allocator>
    struct serializer<binary_archive, vector<T, Allocator>>
    {
        TEMPEST_API
        static auto serialize(binary_archive& archive, const vector<T>& vec) -> void
        {
            const auto size = static_cast<uint64_t>(vec.size());
            serializer<binary_archive, uint64_t>::serialize(archive, size);
            if constexpr (is_trivially_copyable_v<T>)
            {
                const auto byte_span = as_bytes(span{vec.data(), vec.size()});
                archive.write(byte_span);
            }
            else
            {
                for (const auto& item : vec)
                {
                    serializer<binary_archive, T>::serialize(archive, item);
                }
            }
        }

        TEMPEST_API
        static auto deserialize(binary_archive& archive) -> vector<T>
        {
            const auto size = serializer<binary_archive, uint64_t>::deserialize(archive);
            auto vec = vector<T, Allocator>{};

            if constexpr (is_trivially_copyable_v<T>)
            {
                unsafe::resize_no_init(vec, size);
                const auto byte_span = archive.read(size * sizeof(T));
                tempest::memcpy(vec.data(), byte_span.data(), size * sizeof(T));
            }
            else
            {
                vec.reserve(size);
                for (uint64_t i = 0; i < size; ++i)
                {
                    vec.push_back(serializer<binary_archive, T>::deserialize(archive));
                }
            }

            return vec;
        }
    };

    template <typename T>
    struct serializer<binary_archive, optional<T>>
    {
        TEMPEST_API
        static auto serialize(binary_archive& archive, const optional<T>& opt) -> void
        {
            const auto has_value = opt.has_value();
            serializer<binary_archive, bool>::serialize(archive, has_value);
            if (has_value)
            {
                serializer<binary_archive, T>::serialize(archive, opt.value());
            }
        }

        TEMPEST_API
        static auto deserialize(binary_archive& archive) -> optional<T>
        {
            const auto has_value = serializer<binary_archive, bool>::deserialize(archive);
            if (has_value)
            {
                return serializer<binary_archive, T>::deserialize(archive);
            }
            return nullopt;
        }
    };

    template <typename CharT, typename Traits, typename Allocator>
    struct serializer<binary_archive, basic_string<CharT, Traits, Allocator>>
    {
        TEMPEST_API
        static auto serialize(binary_archive& archive, const basic_string<CharT, Traits, Allocator>& str) -> void
        {
            const auto size = static_cast<uint64_t>(str.size());
            serializer<binary_archive, uint64_t>::serialize(archive, size);
            archive.write(as_bytes(span{str.data(), str.size()}));
        }

        TEMPEST_API
        static auto deserialize(binary_archive& archive) -> basic_string<CharT, Traits, Allocator>
        {
            const auto size = serializer<binary_archive, uint64_t>::deserialize(archive);
            auto str = basic_string<CharT, Traits, Allocator>{};
            str.resize(size);
            auto chars = archive.read(size * sizeof(CharT));
            tempest::memcpy(str.data(), chars.data(), size * sizeof(CharT));
            return str;
        }
    };

    template <typename Key, typename Value, typename Hash, typename KeyEqual, typename Allocator>
    struct serializer<binary_archive, flat_unordered_map<Key, Value, Hash, KeyEqual, Allocator>>
    {
        TEMPEST_API
        static auto serialize(binary_archive& archive, const flat_unordered_map<Key, Value, Hash, KeyEqual, Allocator>& map) -> void
        {
            const auto size = static_cast<uint64_t>(map.size());
            
            serializer<binary_archive, uint64_t>::serialize(archive, size);
            
            for (const auto& [key, value] : map)
            {
                serializer<binary_archive, Key>::serialize(archive, key);
                serializer<binary_archive, Value>::serialize(archive, value);
            }
        }

        TEMPEST_API
        static auto deserialize(binary_archive& archive) -> flat_unordered_map<Key, Value, Hash, KeyEqual, Allocator>
        {
            const auto size = serializer<binary_archive, uint64_t>::deserialize(archive);
            auto map = flat_unordered_map<Key, Value, Hash, KeyEqual, Allocator>{};
            
            for (uint64_t i = 0; i < size; ++i)
            {
                auto key = serializer<binary_archive, Key>::deserialize(archive);
                auto value = serializer<binary_archive, Value>::deserialize(archive);
                map.insert({tempest::move(key), tempest::move(value)});
            }

            return map;
        }
    };

    template <archive A>
    class archiver
    {
      public:
        explicit archiver(A& archive) : _archive(&archive)
        {
        }

        template <typename T>
        auto serialize(const T& value) -> void
        {
            serializer<A, T>::serialize(*_archive, value);
        }

        template <typename T>
        auto deserialize() -> T
        {
            return serializer<A, T>::deserialize(*_archive);
        }

      private:
        A* _archive;
    };
} // namespace tempest::serialization

#endif // tempest_serialization_serial_hpp
