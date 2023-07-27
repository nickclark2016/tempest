#ifndef tempest_assets_assets_hpp
#define tempest_assets_assets_hpp

#include <tempest/logger.hpp>
#include <tempest/memory.hpp>
#include <tempest/object_pool.hpp>

#include <tempest/assets/asset.hpp>
#include <tempest/loaders/asset_loader.hpp>

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
#include <utility>

/*

 Base asset class
 Async loading with default asset before real asset is loaded
 Sync loading
 Proper best fit memory allocation


*/

namespace tempest::assets
{
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

    template <typename T>
    concept has_type = requires { typename T::type; };

    struct asset_pool
    {
        asset_pool(core::allocator* alloc, std::uint32_t pool_size, std::uint32_t resource_size)
            : object_pool{alloc, pool_size, resource_size}
        {
        }

        core::object_pool object_pool;
        std::unordered_map<std::string, std::uint32_t> asset_id_to_object_id;
    };

    class asset_manager
    {
      public:
        asset_manager();

        asset_manager(const asset_manager&) = delete;
        asset_manager(asset_manager&& other) noexcept = delete;
        ~asset_manager();

        asset_manager& operator=(const asset_manager&) = delete;
        asset_manager& operator=(asset_manager&& rhs) noexcept = delete;

        //template <std::derived_from<asset> T> T* get(std::string_view name);
        template <std::derived_from<asset> T> T* get(const std::filesystem::path path);

        template <std::derived_from<asset> T> T* load(const std::filesystem::path path);

        template <std::derived_from<asset> T> T* release(std::string_view name);
        template <std::derived_from<asset> T> T* release(const std::filesystem::path path);

        template <typename T, typename... Args>
            requires std::derived_from<T, asset_loader> && has_type<T>
        void register_loader(Args&&... args);

      private:
        std::unordered_map<std::size_t, std::unique_ptr<asset_loader>> _asset_loaders;
        std::unordered_map<std::size_t, asset_pool> _asset_pools;
        core::heap_allocator _alloc;
    };

    template <std::derived_from<asset> T> inline T* assets::asset_manager::get(const std::filesystem::path path)
    {
        //static auto logger = tempest::logger::logger_factory::create({.prefix{"tempest::assets::asset_manager"}});

        static auto asset_id = typeid_counter<T>::value();

        assert(_asset_loaders.contains(asset_id) && "Asset Manager does not contain a loader for this asset type.");

        auto asset_pool_it = _asset_pools.find(asset_id);
        assert(asset_pool_it != _asset_pools.end() &&
               "Asset Manager does not contain a asset pool for this asset type.");

        auto& asset_pool = asset_pool_it->second;
        auto& lud = asset_pool.asset_id_to_object_id;

        auto lud_it = lud.find(path.string());
        assert(lud_it != lud.end() && "Asset Manager does not contain an asset with this path.");

        return reinterpret_cast<T*>(asset_pool.object_pool.access(lud_it->second));
    }

    template <std::derived_from<asset> T> inline T* assets::asset_manager::load(const std::filesystem::path path)
    {
        static auto logger = tempest::logger::logger_factory::create({.prefix{"tempest::assets::asset_manager"}});

        // check if it hasn't been loaded
        static auto asset_id = typeid_counter<T>::value();

        assert(_asset_loaders.contains(asset_id) && "Asset Manager does not contain a loader for this asset type.");
        auto& loader = _asset_loaders[asset_id];

        auto asset_pool_it = _asset_pools.find(asset_id);
        assert(asset_pool_it != _asset_pools.end() &&
               "Asset Manager does not contain a asset pool for this asset type.");

        auto& asset_pool = asset_pool_it->second;
        auto& lud = asset_pool.asset_id_to_object_id;

        auto pool_id = asset_pool.object_pool.acquire_resource();
        auto pool_pointer = asset_pool.object_pool.access(pool_id);

        if (!loader->load(path, pool_pointer))
        {
            // Error during loading
            logger->error("Error during loading of asset.");
            asset_pool.object_pool.release_resource(pool_id);
        }

        auto* asset_pointer = reinterpret_cast<T*>(pool_pointer);

        lud.insert(std::make_pair(path.string(), pool_id));

        return asset_pointer;
    }

    template <typename T, typename... Args>
        requires std::derived_from<T, asset_loader> && has_type<T>
    inline void assets::asset_manager::register_loader(Args&&... args)
    {
        auto id = typeid_counter<typename T::type>::value();

        assert(!_asset_loaders.contains(id) && "Asset Manager already contains a loader for this type.");

        std::unique_ptr<asset_loader> loader = std::make_unique<T>(std::forward<Args>(args)...);
        _asset_loaders[id] = std::move(loader);

        // Figure out better way to determine pool size
        //_asset_pools.insert(std::make_pair(id, asset_pool{.object_pool{&_alloc, 64, sizeof(T)}}));
        _asset_pools.emplace(std::piecewise_construct, std::forward_as_tuple(id),
                             std::forward_as_tuple(&_alloc, 64, static_cast<std::uint32_t>(sizeof(T))));
    }
} // namespace tempest::assets

#endif // tempest_assets_assets_hpp