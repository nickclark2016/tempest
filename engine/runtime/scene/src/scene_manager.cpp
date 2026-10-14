#include <tempest/scene/scene_manager.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/files.hpp>
#include <tempest/random.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/utility.hpp>

namespace tempest::scene
{
    namespace
    {
        auto generate_random_local_id() -> entity_local_id
        {
            auto device = random_device{};
            auto val = (static_cast<uint64_t>(device()) << 32) | static_cast<uint64_t>(device());
            auto local_id = entity_local_id{val};
            while (!local_id.is_valid())
            {
                val = (static_cast<uint64_t>(device()) << 32) | static_cast<uint64_t>(device());
                local_id = entity_local_id{val};
            }
            return local_id;
        }
    } // namespace

    scene_manager::scene_manager(ecs::archetype_registry& registry)
        : scene_manager(registry, nullptr, nullptr)
    {
    }

    scene_manager::scene_manager(ecs::archetype_registry& registry, assets::asset_database* assets,
                                 job::job_system* jobs)
        : _registry{registry}, _assets{assets}, _jobs{jobs}
    {
        _registry->type_registry().ensure<scene_inactive_tag>();
        _registry->type_registry().ensure<scene_entity_component>();
    }

    scene_manager::~scene_manager()
    {
        unload_all();
    }

    auto scene_manager::is_valid(scene_handle handle) const noexcept -> bool
    {
        if (handle.index >= _scenes.size())
        {
            return false;
        }
        const auto& slot = _scenes[handle.index];
        return slot.occupied && (slot.generation == handle.generation);
    }

    auto scene_manager::get_scene_count() const noexcept -> size_t
    {
        return _occupied_count;
    }

    auto scene_manager::get_root_entities(scene_handle handle) const -> optional<span<const ecs::entity>>
    {
        if (!is_valid(handle))
        {
            return nullopt;
        }
        const auto& slot = _scenes[handle.index];
        return optional<span<const ecs::entity>>{
            span<const ecs::entity>{slot.root_entities.data(), slot.root_entities.size()}};
    }

    auto scene_manager::get_scene_path(scene_handle handle) const -> optional<string_view>
    {
        if (!is_valid(handle))
        {
            return nullopt;
        }
        const auto& slot = _scenes[handle.index];
        if (slot.path.empty())
        {
            return nullopt;
        }
        return optional<string_view>{string_view{slot.path}};
    }

    auto scene_manager::get_scene_name(scene_handle handle) const -> optional<string_view>
    {
        if (!is_valid(handle))
        {
            return nullopt;
        }
        const auto& slot = _scenes[handle.index];
        if (slot.name.empty())
        {
            return nullopt;
        }
        return optional<string_view>{string_view{slot.name}};
    }

    auto scene_manager::set_active_scene(scene_handle handle) -> bool
    {
        if (!handle.is_valid())
        {
            _active_scene = default_active_scene;
            return true;
        }

        if (is_valid(handle))
        {
            _active_scene = handle;
            return true;
        }

        return false;
    }

    auto scene_manager::get_active_scene() const noexcept -> scene_handle
    {
        return _active_scene;
    }

    auto scene_manager::is_dirty(scene_handle handle) const noexcept -> bool
    {
        if (!is_valid(handle))
        {
            return false;
        }
        return _scenes[handle.index].is_dirty;
    }

    auto scene_manager::set_dirty(scene_handle handle, bool dirty) -> bool
    {
        if (!is_valid(handle))
        {
            return false;
        }
        _scenes[handle.index].is_dirty = dirty;
        return true;
    }

    auto scene_manager::create_entity(scene_handle handle, string_view name) -> ecs::entity
    {
        if (!is_valid(handle))
        {
            return ecs::tombstone;
        }

        const auto ent = _registry->create();
        const auto local_id = generate_random_local_id();
        _registry->assign(ent, scene_entity_component{
                                   .scene = handle,
                                   .local_id = local_id,
                               });

        if (!name.empty())
        {
            _registry->name(ent, name);
        }

        _scenes[handle.index].root_entities.push_back(ent);
        return ent;
    }

    auto scene_manager::allocate_slot() -> scene_handle
    {
        if (!_free_slots.empty())
        {
            const auto slot_index = _free_slots.back();
            _free_slots.pop_back();
            auto& slot = _scenes[slot_index];
            slot.occupied = true;
            slot.is_dirty = scene_record::default_dirty;
            ++_occupied_count;
            return scene_handle{
                .index = slot_index,
                .generation = slot.generation,
            };
        }

        const auto slot_index = static_cast<uint32_t>(_scenes.size());
        auto& slot = _scenes.emplace_back();
        slot.occupied = true;
        slot.generation = 1;
        slot.is_dirty = scene_record::default_dirty;
        ++_occupied_count;
        return scene_handle{
            .index = slot_index,
            .generation = slot.generation,
        };
    }

    auto scene_manager::free_slot(uint32_t index) -> void
    {
        auto& slot = _scenes[index];
        slot.occupied = false;
        slot.generation++;
        slot.is_dirty = scene_record::default_dirty;
        slot.path.clear();
        slot.name.clear();
        slot.root_entities.clear();
        slot.unrecognized_data = {};
        _free_slots.push_back(index);
        --_occupied_count;
    }

    auto scene_manager::load_sync(const filesystem::path& path, const scene_load_options& options)
        -> expected<scene_handle, scene_error>
    {
        const auto content_res = read_file_to_string(path);
        if (!content_res.has_value())
        {
            if (content_res.error() == file_error::not_found)
            {
                return unexpected(scene_error::file_not_found);
            }
            return unexpected(scene_error::io_error);
        }

        const auto scene_name = path.stem().string();
        auto handle_res = load_from_string_sync(*content_res, scene_name, options);
        if (handle_res.has_value())
        {
            _scenes[handle_res->index].path = path.generic_string();
        }
        return handle_res;
    }

    auto scene_manager::load_from_string_sync(string_view json_content, string_view scene_name,
                                              const scene_load_options& options)
        -> expected<scene_handle, scene_error>
    {
        if (options.mode == scene_load_mode::single && options.clear_first)
        {
            unload_all();
        }

        const auto handle = allocate_slot();
        auto result = scene_codec::deserialize(*_registry, handle, json_content);

        if (!result.success)
        {
            // Rollback: destroy any partially instantiated entities
            for (const auto ent : result.all_entities)
            {
                if (_registry->is_valid(ent))
                {
                    _registry->destroy_recursive(ent);
                }
            }

            auto sweep_entities = vector<ecs::entity>{};
            for (const auto [self, comp] : _registry->with<ecs::self_component, scene_entity_component>())
            {
                if (comp.scene == handle)
                {
                    sweep_entities.push_back(self.entity);
                }
            }
            for (const auto ent : sweep_entities)
            {
                if (_registry->is_valid(ent))
                {
                    _registry->destroy_recursive(ent);
                }
            }

            free_slot(handle.index);
            return unexpected(scene_error::deserialization_failed);
        }

        auto& record = _scenes[handle.index];
        record.root_entities = tempest::move(result.root_entities);
        record.unrecognized_data = tempest::move(result.unrecognized_data);
        if (!scene_name.empty())
        {
            record.name = string{scene_name};
        }

        if (options.mode == scene_load_mode::single && !options.clear_first)
        {
            // Atomic replacement: unload all prior scenes after successful loading
            for (auto prior_index = 0U; prior_index < static_cast<uint32_t>(_scenes.size()); ++prior_index)
            {
                if (prior_index != handle.index && _scenes[prior_index].occupied)
                {
                    unload(scene_handle{
                        .index = prior_index,
                        .generation = _scenes[prior_index].generation,
                    });
                }
            }
        }

        if (!_active_scene.is_valid())
        {
            set_active_scene(handle);
        }

        return handle;
    }

    auto scene_manager::unload(scene_handle handle) -> bool
    {
        if (!is_valid(handle))
        {
            return false;
        }

        // Destroy all root entities and subtrees recursively
        auto roots_to_destroy = tempest::move(_scenes[handle.index].root_entities);
        for (const auto root : roots_to_destroy)
        {
            if (_registry->is_valid(root))
            {
                _registry->destroy_recursive(root);
            }
        }

        // Sweep: query any remaining entities with scene_entity_component where comp.scene == handle
        auto sweep_entities = vector<ecs::entity>{};
        for (const auto [self, comp] : _registry->with<ecs::self_component, scene_entity_component>())
        {
            if (comp.scene == handle)
            {
                sweep_entities.push_back(self.entity);
            }
        }
        for (const auto ent : sweep_entities)
        {
            if (_registry->is_valid(ent))
            {
                _registry->destroy_recursive(ent);
            }
        }

        // Reset slot in _scenes, increment generation, push index to free list
        free_slot(handle.index);

        // If _active_scene == handle, clear _active_scene
        if (_active_scene == handle)
        {
            _active_scene = default_active_scene;
        }

        return true;
    }

    auto scene_manager::unload_all() -> void
    {
        for (auto i = 0U; i < static_cast<uint32_t>(_async_ops.size()); ++i)
        {
            if (_async_ops[i].occupied && (_async_ops[i].state == scene_load_state::decoding ||
                                           _async_ops[i].state == scene_load_state::resolving_assets ||
                                           _async_ops[i].state == scene_load_state::committing))
            {
                cancel_load(scene_load_handle{
                    .index = i,
                    .generation = _async_ops[i].generation,
                    .manager = this,
                });
            }
        }

        for (auto slot_index = 0U; slot_index < static_cast<uint32_t>(_scenes.size()); ++slot_index)
        {
            if (_scenes[slot_index].occupied)
            {
                unload(scene_handle{
                    .index = slot_index,
                    .generation = _scenes[slot_index].generation,
                });
            }
        }
    }

    auto scene_manager::save_sync(scene_handle handle, const filesystem::path& path,
                                  const scene_serialize_options& options) -> expected<void, scene_error>
    {
        const auto str_res = save_to_string(handle, options);
        if (!str_res.has_value())
        {
            return unexpected(str_res.error());
        }

        const auto byte_span =
            span<const byte>{reinterpret_cast<const byte*>(str_res->data()), str_res->size()};

        const auto write_res = write_file_from_bytes(path, byte_span);
        if (!write_res.has_value())
        {
            _scenes[handle.index].is_dirty = true;
            return unexpected(scene_error::io_error);
        }

        _scenes[handle.index].path = path.generic_string();
        if (_scenes[handle.index].name.empty())
        {
            _scenes[handle.index].name = path.stem().string();
        }
        _scenes[handle.index].is_dirty = false;

        return {};
    }

    auto scene_manager::save_to_string(scene_handle handle, const scene_serialize_options& options)
        -> expected<string, scene_error>
    {
        if (!is_valid(handle))
        {
            return unexpected(scene_error::invalid_handle);
        }

        auto& record = _scenes[handle.index];

        auto valid_roots = vector<ecs::entity>{};
        for (const auto root : record.root_entities)
        {
            if (_registry->is_valid(root))
            {
                const auto* rel = _registry->try_get<ecs::relationship_component<ecs::entity>>(root);
                if (rel == nullptr || rel->parent == ecs::tombstone || rel->parent == ecs::null ||
                    !_registry->is_valid(rel->parent))
                {
                    valid_roots.push_back(root);
                }
            }
        }

        auto serialized = scene_codec::serialize(*_registry, valid_roots, record.unrecognized_data, options);
        record.is_dirty = false;
        return serialized;
    }
} // namespace tempest::scene
