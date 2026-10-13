#ifndef tempest_scene_prefab_cache_hpp
#define tempest_scene_prefab_cache_hpp

#include <tempest/api.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/mutex.hpp>
#include <tempest/optional.hpp>
#include <tempest/scene/prefab_template.hpp>

namespace tempest::scene
{
    /// @brief Thread-safe cache storing immutable prefab templates indexed by GUID.
    class TEMPEST_API prefab_cache
    {
      public:
        prefab_cache() = default;
        prefab_cache(const prefab_cache&) = delete;
        prefab_cache(prefab_cache&&) = delete;
        ~prefab_cache() = default;

        auto operator=(const prefab_cache&) -> prefab_cache& = delete;
        auto operator=(prefab_cache&&) -> prefab_cache& = delete;

        auto register_template(const guid& tid, prefab_template tmpl) -> bool;
        auto unregister_template(const guid& tid) -> bool;
        [[nodiscard]] auto has_template(const guid& tid) const -> bool;
        [[nodiscard]] auto find_template(const guid& tid) const -> optional<prefab_template>;

        template <typename Func>
        auto with_template(const guid& tid, Func&& func) const -> bool
        {
            auto lock = shared_lock{_mutex};
            const auto iter = _templates.find(tid);
            if (iter != _templates.end())
            {
                func(iter->second);
                return true;
            }
            return false;
        }

        [[nodiscard]] auto template_count() const -> size_t;
        auto clear() -> void;

      private:
        mutable shared_mutex _mutex;
        flat_unordered_map<guid, prefab_template> _templates;
    };
} // namespace tempest::scene

#endif // tempest_scene_prefab_cache_hpp
