#ifndef tempest_assets_assets_hpp
#define tempest_assets_assets_hpp

#include <tempest/logger.hpp>
#include <tempest/object_pool.hpp>
#include <tempest/memory.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

/*

 Base asset class
 Async loading with default asset before real asset is loaded
 Sync loading
 Proper best fit memory allocation


*/

namespace tempest::assets
{
    class asset_manager;

    struct counter
    {
        static std::size_t value;
    };

    template <typename T> struct typeid_counter : private counter
    {
        static std::size_t value()
        {
            static std::size_t v = counter::value++;
            return v;
        }
    };

    struct asset_metadata
    {
        std::string name;
        std::uint64_t uuid;
        std::optional<std::filesystem::path> path;
    };

    class asset
    {
      public:
        asset(std::string_view name)
        {
            _metadata.name = name;
        }
        asset(const asset&) = delete;
        asset(asset&& other) noexcept = default;
        ~asset() = default;

        asset& operator=(const asset&) = delete;
        asset& operator=(asset&& rhs) noexcept = default;

      private:
        friend class asset_manager;

        asset_metadata _metadata;
    };

    class asset_loader
    {
      public:
        asset_loader() = default;
        asset_loader(const asset_loader&) = default;
        asset_loader(asset_loader&& other) noexcept = default;
        ~asset_loader() = default;

        asset_loader& operator=(const asset_loader&) = default;
        asset_loader& operator=(asset_loader&& rhs) noexcept = default;

        virtual bool load(const std::filesystem::path& path, void* dest) = 0;
        // std::unique_ptr<asset> load_async();

        virtual bool release(std::unique_ptr<asset>&& asset) = 0;
    };

    template <typename T>
    concept has_type = requires { typename T::type; };

    struct asset_pool
    {
        asset_pool(core::allocator* alloc, std::uint32_t pool_size, std::uint32_t resource_size)
            : object_pool{alloc, pool_size, resource_size}
        {
        }

        core::object_pool object_pool;
        std::unordered_map<std::size_t, std::uint32_t> _asset_id_to_object_id;
    };

    class asset_manager
    {
      public:
        asset_manager() : _alloc{64 * 1024}
        {
        }

        asset_manager(const asset_manager&) = delete;
        asset_manager(asset_manager&& other) noexcept = delete;
        ~asset_manager()
        {
            for (auto& [id, pool] : _asset_pools)
            {
                pool.object_pool.release_all_resources();
            }
        }

        asset_manager& operator=(const asset_manager&) = delete;
        asset_manager& operator=(asset_manager&& rhs) noexcept = delete;

        template <std::derived_from<asset> T> T* get(std::string_view name);
        template <std::derived_from<asset> T> T* get(const std::filesystem::path path);

        template <std::derived_from<asset> T> T* load(const std::filesystem::path path);

        template <std::derived_from<asset> T> T* release(std::string_view name);
        template <std::derived_from<asset> T> T* release(const std::filesystem::path path);

        template <typename T>
            requires std::derived_from<T, asset_loader> && std::default_initializable<T> && has_type<T>
        void register_loader();

      private:
        std::unordered_map<std::size_t, std::unique_ptr<asset_loader>> _asset_loaders;
        std::unordered_map<std::size_t, asset_pool> _asset_pools;
        core::heap_allocator _alloc;
    };

    template <std::derived_from<asset> T> inline T* assets::asset_manager::load(const std::filesystem::path path)
    {
        static auto logger = tempest::logger::logger_factory::create({.prefix{"tempest::assets::asset_manager"}});

        // check if it hasn't been loaded
        static auto asset_id = typeid_counter<T>::value();

        assert(_asset_loaders.contains(asset_id) && "Asset Manager does not contain a loader for this asset type.");
        auto& loader = _asset_loaders[asset_id];

        auto asset_pool_it = _asset_pools.find(asset_id);
        assert(asset_pool_it != _asset_pools.end() && "Asset Manager does not contain a asset pool for this asset type.");

        auto& asset_pool = asset_pool_it->second;

        auto pool_id = asset_pool.object_pool.acquire_resource();
        auto pool_pointer = asset_pool.object_pool.access(pool_id);

        if (!loader->load(path, pool_pointer))
        {
            // Error during loading
            logger->error("Error during loading of asset.");
            asset_pool.object_pool.release_resource(pool_id);
        }

        return reinterpret_cast<T*>(pool_pointer);
    }

    template <typename T>
        requires std::derived_from<T, asset_loader> && std::default_initializable<T> && has_type<T>
    inline void assets::asset_manager::register_loader()
    {
        auto id = typeid_counter<typename T::type>::value();

        assert(!_asset_loaders.contains(id) && "Asset Manager already contains a loader for this type.");

        std::unique_ptr<asset_loader> loader = std::make_unique<T>();
        _asset_loaders[id] = std::move(loader);

        // Figure out better way to determine pool size
        //_asset_pools.insert(std::make_pair(id, asset_pool{.object_pool{&_alloc, 64, sizeof(T)}}));
        _asset_pools.emplace(std::piecewise_construct, std::forward_as_tuple(id),
                             std::forward_as_tuple(&_alloc, 64, static_cast<std::uint32_t>(sizeof(T))));
    }
} // namespace tempest::assets

#endif // tempest_assets_assets_hpp