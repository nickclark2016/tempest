#include <tempest/component_type_registry.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/span.hpp>

namespace tempest::ecs
{
    auto component_type_registry::type_index(string_view name) const -> optional<uint32_t>
    {
        const auto resolved = resolve_alias(name);
        const auto it = _name_to_index.find(string{resolved});
        if (it != _name_to_index.end())
        {
            return it->second;
        }
        return nullopt;
    }

    auto component_type_registry::type_info(uint32_t index) const -> const component_type_info&
    {
        TEMPEST_ASSERT(index < _count);
        return _types[index];
    }

    auto component_type_registry::type_info(string_view name) const -> const component_type_info*
    {
        const auto opt_idx = type_index(name);
        if (!opt_idx.has_value())
        {
            return nullptr;
        }
        return &_types[*opt_idx];
    }

    auto component_type_registry::descriptor(uint32_t index) const -> const type_descriptor*
    {
        if (index >= _count)
        {
            return nullptr;
        }
        const auto& info = _types[index];
        if (info.descriptor.has_value())
        {
            return &(*info.descriptor);
        }
        return nullptr;
    }

    auto component_type_registry::descriptor(string_view name) const -> const type_descriptor*
    {
        const auto opt_idx = type_index(name);
        if (!opt_idx.has_value())
        {
            return nullptr;
        }
        return descriptor(*opt_idx);
    }

    void component_type_registry::add_alias(string_view legacy_name, string_view current_name)
    {
        _aliases[string{legacy_name}] = string{current_name};
    }

    auto component_type_registry::resolve_alias(string_view name) const -> string_view
    {
        auto current = name;
        for (auto iteration = size_t{0}; iteration < _aliases.size(); ++iteration)
        {
            const auto it = _aliases.find(string{current});
            if (it == _aliases.end())
            {
                break;
            }
            current = string_view{it->second};
        }
        return current;
    }

    auto component_type_registry::size() const noexcept -> size_t
    {
        return _count;
    }

    auto component_type_registry::empty() const noexcept -> bool
    {
        return _count == 0;
    }

    namespace
    {
        static constexpr uint64_t fnv_offset = 14695981039346656037ULL;
        static constexpr uint64_t fnv_prime = 1099511628211ULL;
        static constexpr size_t bytes_per_u64 = 8;
        static constexpr size_t bytes_per_u32 = 4;
        static constexpr size_t bits_per_byte = 8;

        auto compute_schema_hash(span<const type_manifest_entry> types) noexcept -> uint64_t
        {
            auto hash = fnv_offset;
            auto hash_byte = [&hash](uint8_t byte_val) noexcept {
                hash ^= static_cast<uint64_t>(byte_val);
                hash *= fnv_prime;
            };
            auto hash_u64 = [&hash_byte](uint64_t val) noexcept {
                for (auto shift = size_t{0}; shift < bytes_per_u64; ++shift)
                {
                    hash_byte(static_cast<uint8_t>(val >> (shift * bits_per_byte)));
                }
            };
            auto hash_u32 = [&hash_byte](uint32_t val) noexcept {
                for (auto shift = size_t{0}; shift < bytes_per_u32; ++shift)
                {
                    hash_byte(static_cast<uint8_t>(val >> (shift * bits_per_byte)));
                }
            };
            auto hash_str = [&hash_byte](string_view str) noexcept {
                for (const auto character : str)
                {
                    hash_byte(static_cast<uint8_t>(character));
                }
            };

            for (const auto& entry : types)
            {
                if (!entry.is_serializable)
                {
                    continue;
                }

                hash_str(entry.name);
                hash_u64(static_cast<uint64_t>(entry.size));
                hash_u64(static_cast<uint64_t>(entry.alignment));
                hash_u32(entry.version);

                for (const auto& field : entry.fields)
                {
                    hash_str(field.name);
                    hash_byte(static_cast<uint8_t>(field.kind));
                    hash_byte(static_cast<uint8_t>(field.flags));
                    hash_u64(static_cast<uint64_t>(field.offset));
                    hash_u64(static_cast<uint64_t>(field.size));
                }
            }

            return hash;
        }
    } // namespace

    auto component_type_registry::schema_hash() const noexcept -> uint64_t
    {
        const auto manifest = export_manifest();
        return manifest.schema_hash;
    }

    auto component_type_registry::export_manifest() const -> type_manifest
    {
        auto manifest = type_manifest{};
        manifest.types.reserve(_count);

        for (auto type_idx = size_t{0}; type_idx < _count; ++type_idx)
        {
            const auto& info = _types[type_idx];
            auto entry = type_manifest_entry{
                .name = string{info.name},
                .size = static_cast<size_t>(info.size),
                .alignment = static_cast<size_t>(info.alignment),
                .version = info.descriptor.has_value() ? info.descriptor->version : uint32_t{1},
                .is_serializable = info.is_serializable,
                .is_authorable = info.is_authorable,
                .fields = {},
            };

            if (info.descriptor.has_value())
            {
                entry.fields.reserve(info.descriptor->fields.size());
                for (const auto& field_desc : info.descriptor->fields)
                {
                    entry.fields.push_back(type_manifest_field{
                        .name = string{field_desc.name},
                        .kind = field_desc.kind,
                        .flags = field_desc.flags,
                        .offset = field_desc.offset,
                        .size = field_desc.size,
                    });
                }

                tempest::sort(entry.fields.begin(), entry.fields.end(),
                              [](const type_manifest_field& lhs, const type_manifest_field& rhs) {
                                  if (lhs.offset != rhs.offset)
                                  {
                                      return lhs.offset < rhs.offset;
                                  }
                                  return lhs.name < rhs.name;
                              });
            }

            manifest.types.push_back(tempest::move(entry));
        }

        tempest::sort(manifest.types.begin(), manifest.types.end(),
                      [](const type_manifest_entry& lhs, const type_manifest_entry& rhs) {
                          return lhs.name < rhs.name;
                      });

        manifest.schema_hash = compute_schema_hash(manifest.types);
        return manifest;
    }

    auto component_type_registry::register_type(component_type_info info) -> uint32_t
    {
        const auto resolved = resolve_alias(info.name);
        const auto existing = type_index(resolved);
        if (existing.has_value())
        {
            return *existing;
        }

        TEMPEST_ASSERT(_count < max_component_types);
        const auto idx = static_cast<uint32_t>(_count);
        info.index = idx;
        info.name = resolved;
        _register_type_info(tempest::move(info));
        return idx;
    }

    void component_type_registry::_register_type_info(component_type_info info)
    {
        TEMPEST_ASSERT(_count < max_component_types);
        const auto idx = info.index;
        _type_names[idx] = string{info.name};
        info.name = string_view{_type_names[idx]};
        _name_to_index[_type_names[idx]] = idx;
        _types[idx] = tempest::move(info);
        ++_count;
    }
} // namespace tempest::ecs
