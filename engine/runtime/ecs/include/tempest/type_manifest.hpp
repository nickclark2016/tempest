#ifndef tempest_ecs_type_manifest_hpp
#define tempest_ecs_type_manifest_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/json.hpp>
#include <tempest/reflection.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::ecs
{
    struct TEMPEST_API type_manifest_field
    {
        static constexpr field_kind default_kind = field_kind::boolean;
        static constexpr field_flags default_flags = field_flags::none;
        static constexpr size_t default_offset = field_descriptor::no_offset;
        static constexpr size_t default_size = 0;

        string name;
        field_kind kind = default_kind;
        field_flags flags = default_flags;
        size_t offset = default_offset;
        size_t size = default_size;
    };

    struct TEMPEST_API type_manifest_entry
    {
        static constexpr size_t default_size = 0;
        static constexpr size_t default_alignment = 0;
        static constexpr uint32_t default_version = 1;
        static constexpr bool default_is_serializable = true;
        static constexpr bool default_is_authorable = true;

        string name;
        size_t size = default_size;
        size_t alignment = default_alignment;
        uint32_t version = default_version;
        bool is_serializable = default_is_serializable;
        bool is_authorable = default_is_authorable;
        vector<type_manifest_field> fields;
    };

    struct TEMPEST_API type_manifest
    {
        static constexpr uint64_t default_schema_hash = 0;

        uint64_t schema_hash = default_schema_hash;
        vector<type_manifest_entry> types;

        [[nodiscard]] auto to_json(core::json_format fmt = core::json_format::pretty) const -> string;
    };
} // namespace tempest::ecs

#endif // tempest_ecs_type_manifest_hpp
