#ifndef tempest_assets_project_workspace_hpp
#define tempest_assets_project_workspace_hpp

#include <tempest/api.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets
{
    struct TEMPEST_API mount_point
    {
        static constexpr int32_t default_priority = 0;

        string path = {};
        int32_t priority = default_priority;
        string alias = {};

        auto operator==(const mount_point& rhs) const noexcept -> bool = default;
    };

    struct TEMPEST_API bake_settings
    {
        static constexpr string_view default_output_directory = "baked";
        static constexpr bool default_compress_textures = false;

        string output_directory = string{default_output_directory};
        bool compress_textures = default_compress_textures;
        flat_unordered_map<string, string> custom_settings = {};

        auto operator==(const bake_settings& rhs) const noexcept -> bool = default;
    };

    struct TEMPEST_API startup_scene_info
    {
        optional<guid> scene_guid = nullopt;
        string scene_path = {};

        auto operator==(const startup_scene_info& rhs) const noexcept -> bool = default;
    };

    struct TEMPEST_API sidecar_meta
    {
        static constexpr uint32_t default_importer_version = 1;

        guid source_guid = {};
        string importer = {};
        uint32_t importer_version = default_importer_version;
        flat_unordered_map<string, string> settings = {};
        flat_unordered_map<string, guid> sub_assets = {};
        flat_unordered_map<string, asset_type_id> sub_asset_types = {};

        [[nodiscard]] auto resolve_or_create_sub_asset(string_view sub_asset_key,
                                                       optional<asset_type_id> type = nullopt) -> guid;

        [[nodiscard]] static auto generate_deterministic_guid(const guid& source_guid,
                                                              string_view sub_asset_key) noexcept -> guid;

        [[nodiscard]] auto save_to_file(string_view meta_path) const -> bool;
        [[nodiscard]] static auto load_from_file(string_view meta_path) -> optional<sidecar_meta>;

        [[nodiscard]] static auto load_from_string(string_view json_str) -> optional<sidecar_meta>;
        [[nodiscard]] auto to_string() const -> string;

        auto operator==(const sidecar_meta& rhs) const noexcept -> bool = default;
    };

    class TEMPEST_API project_workspace
    {
      public:
        string name{};
        string root_directory{};
        vector<mount_point> mounts{};
        startup_scene_info startup_scene{};
        bake_settings settings{};

        [[nodiscard]] auto cache_directory() const -> string;
        [[nodiscard]] auto cache_db_path() const -> string;

        [[nodiscard]] auto load_from_file(string_view tproject_path) -> bool;
        [[nodiscard]] auto save_to_file(string_view tproject_path) const -> bool;

        [[nodiscard]] static auto load_from_string(string_view json_str) -> optional<project_workspace>;
        [[nodiscard]] auto to_string() const -> string;

        auto operator==(const project_workspace& rhs) const noexcept -> bool = default;
    };
} // namespace tempest::assets

#endif // tempest_assets_project_workspace_hpp
