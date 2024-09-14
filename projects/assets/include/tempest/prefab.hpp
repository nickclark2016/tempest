#ifndef tempest_asset_prefab_hpp
#define tempest_asset_prefab_hpp

#include <tempest/asset.hpp>
#include <tempest/guid.hpp>
#include <tempest/memory.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets
{
    struct prefab
    {
        string name;
        vector<unique_ptr<asset>> assets;
    };

    class prefab_node : public asset
    {
      public:
        explicit prefab_node(string name);
        prefab_node(const prefab_node&) = delete;
        prefab_node(prefab_node&&) noexcept = delete;
        ~prefab_node() override = default;

        prefab_node& operator=(const prefab_node&) = delete;
        prefab_node& operator=(prefab_node&&) noexcept = delete;

        string_view name() const noexcept override;
        guid id() const noexcept override;

        vector<guid>& children() noexcept;
        span<const guid> children() const noexcept;

        math::vec3<float>& position() noexcept;
        const math::vec3<float>& position() const noexcept;

        math::vec3<float>& rotation() noexcept;
        const math::vec3<float>& rotation() const noexcept;

        math::vec3<float>& scale() noexcept;
        const math::vec3<float>& scale() const noexcept;

      private:
        string _name;
        guid _id;
        vector<guid> _children;

        math::vec3<float> _position;
        math::vec3<float> _rotation;
        math::vec3<float> _scale;
    };

    inline prefab_node::prefab_node(string name) : _name(std::move(name))
    {
        _id = guid::generate_random_guid();
    }

    inline string_view prefab_node::name() const noexcept
    {
        return _name;
    }

    inline guid prefab_node::id() const noexcept
    {
        return _id;
    }

    inline vector<guid>& prefab_node::children() noexcept
    {
        return _children;
    }

    inline span<const guid> prefab_node::children() const noexcept
    {
        return _children;
    }

    inline math::vec3<float>& prefab_node::position() noexcept
    {
        return _position;
    }

    inline const math::vec3<float>& prefab_node::position() const noexcept
    {
        return _position;
    }

    inline math::vec3<float>& prefab_node::rotation() noexcept
    {
        return _rotation;
    }

    inline const math::vec3<float>& prefab_node::rotation() const noexcept
    {
        return _rotation;
    }

    inline math::vec3<float>& prefab_node::scale() noexcept
    {
        return _scale;
    }

    inline const math::vec3<float>& prefab_node::scale() const noexcept
    {
        return _scale;
    }
} // namespace tempest::assets

#endif // tempest_asset_prefab_hpp