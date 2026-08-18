#include <tempest/algorithm.hpp>
#include <tempest/deque.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/render_graph/dag.hpp>

namespace tempest::render_graph
{
    auto dag_compiler::register_texture(const rg_texture_desc& desc) -> rg_texture_id
    {
        const auto id = static_cast<uint32_t>(_textures.size());
        _textures.push_back(registered_texture{
            .id = id,
            .desc = desc,
            .is_imported = false,
            .imported_handle = {},
            .initial_layout = rhi::image_layout::undefined,
        });

        return rg_texture_id{
            .id = id,
            .version = 0,
        };
    }

    auto dag_compiler::register_buffer(const rg_buffer_desc& desc) -> rg_buffer_id
    {
        const auto id = static_cast<uint32_t>(_buffers.size());
        _buffers.push_back(registered_buffer{
            .id = id,
            .desc = desc,
            .is_imported = false,
            .imported_handle = {},
        });

        return rg_buffer_id{
            .id = id,
            .version = 0,
        };
    }

    auto dag_compiler::import_texture(rhi::texture_handle handle, rhi::texture_view_handle view,
                                      rhi::image_layout initial_layout) -> rg_texture_id
    {
        const auto id = static_cast<uint32_t>(_textures.size());
        _textures.push_back(registered_texture{
            .id = id,
            .desc = {},
            .is_imported = true,
            .imported_handle = handle,
            .imported_view = view,
            .initial_layout = initial_layout,
        });

        return rg_texture_id{
            .id = id,
            .version = 0,
        };
    }

    auto dag_compiler::import_texture(rhi::texture_handle handle, rhi::image_layout initial_layout) -> rg_texture_id
    {
        return import_texture(handle, {}, initial_layout);
    }

    auto dag_compiler::import_buffer(rhi::buffer_handle handle) -> rg_buffer_id
    {
        const auto id = static_cast<uint32_t>(_buffers.size());
        _buffers.push_back(registered_buffer{
            .id = id,
            .desc = {},
            .is_imported = true,
            .imported_handle = handle,
        });

        return rg_buffer_id{
            .id = id,
            .version = 0,
        };
    }

    auto dag_compiler::add_pass(pass_node node) -> uint32_t
    {
        const auto index = static_cast<uint32_t>(_passes.size());
        node.pass_index = index;
        _passes.push_back(tempest::move(node));
        return index;
    }

    void dag_compiler::reset()
    {
        _textures.clear();
        _buffers.clear();
        _passes.clear();
    }

    auto dag_compiler::compile() const -> expected<compiled_dag, dag_compile_error>
    {
        if (_passes.empty())
        {
            return compiled_dag{};
        }

        auto result = compiled_dag{};

        // 1. Evaluate pass enablement and gather aliases from disabled passes
        auto pass_enabled = vector<bool>(_passes.size(), true);
        for (size_t i = 0; i < _passes.size(); ++i)
        {
            const auto& pass = _passes[i];
            if (pass.enable_condition && !pass.enable_condition())
            {
                pass_enabled[i] = false;
                for (const auto& [produced, fallback] : pass.texture_fallbacks)
                {
                    result.resolved_texture_aliases[produced] = fallback;
                }
                for (const auto& [produced, fallback] : pass.buffer_fallbacks)
                {
                    result.resolved_buffer_aliases[produced] = fallback;
                }
            }
        }

        // Transitive alias resolution
        auto resolve_texture = [&](rg_texture_id id) -> rg_texture_id {
            auto current = id;
            while (true)
            {
                auto it = result.resolved_texture_aliases.find(current);
                if (it == result.resolved_texture_aliases.end() || it->second == current)
                {
                    break;
                }
                current = it->second;
            }
            return current;
        };

        auto resolve_buffer = [&](rg_buffer_id id) -> rg_buffer_id {
            auto current = id;
            while (true)
            {
                auto it = result.resolved_buffer_aliases.find(current);
                if (it == result.resolved_buffer_aliases.end() || it->second == current)
                {
                    break;
                }
                current = it->second;
            }
            return current;
        };

        // 2. Map resource versions to producing pass index
        auto texture_producer = flat_unordered_map<rg_texture_id, uint32_t>{};
        auto buffer_producer = flat_unordered_map<rg_buffer_id, uint32_t>{};

        for (size_t i = 0; i < _passes.size(); ++i)
        {
            if (!pass_enabled[i])
            {
                continue;
            }

            const auto& pass = _passes[i];
            const auto pass_idx = static_cast<uint32_t>(i);

            for (const auto& out_tex : pass.texture_outputs)
            {
                const auto resolved = resolve_texture(out_tex);
                texture_producer[resolved] = pass_idx;
            }

            for (const auto& out_buf : pass.buffer_outputs)
            {
                const auto resolved = resolve_buffer(out_buf);
                buffer_producer[resolved] = pass_idx;
            }
        }

        // 3. Backward reachability from sink passes (Dead-Code Culling)
        auto active_passes = vector<bool>(_passes.size(), false);
        auto work_queue = deque<uint32_t>{};

        for (size_t i = 0; i < _passes.size(); ++i)
        {
            if (pass_enabled[i] && _passes[i].is_sink)
            {
                active_passes[i] = true;
                work_queue.push_back(static_cast<uint32_t>(i));
            }
        }

        while (!work_queue.empty())
        {
            const auto current_pass_idx = work_queue.front();
            work_queue.pop_front();

            const auto& pass = _passes[current_pass_idx];

            for (const auto& acc : pass.texture_accesses)
            {
                if (acc.type == access_type::read || acc.load_op == rhi::load_op::load || acc.texture.version > 0)
                {
                    const auto resolved = resolve_texture(acc.texture);
                    auto it = texture_producer.find(resolved);
                    if (it != texture_producer.end())
                    {
                        const auto prod_idx = it->second;
                        if (!active_passes[prod_idx])
                        {
                            active_passes[prod_idx] = true;
                            work_queue.push_back(prod_idx);
                        }
                    }
                }
            }

            for (const auto& acc : pass.buffer_accesses)
            {
                if (acc.type == access_type::read || acc.buffer.version > 0)
                {
                    const auto resolved = resolve_buffer(acc.buffer);
                    auto it = buffer_producer.find(resolved);
                    if (it != buffer_producer.end())
                    {
                        const auto prod_idx = it->second;
                        if (!active_passes[prod_idx])
                        {
                            active_passes[prod_idx] = true;
                            work_queue.push_back(prod_idx);
                        }
                    }
                }
            }
        }

        // If no sink pass was explicitly marked, and no passes are active, check if all enabled passes should run
        auto active_count = size_t{0};
        for (size_t i = 0; i < active_passes.size(); ++i)
        {
            if (active_passes[i])
            {
                ++active_count;
            }
        }

        if (active_count == 0)
        {
            // Fallback: If no sink was marked, treat all enabled passes as active (useful for testing or full graphs)
            for (size_t i = 0; i < _passes.size(); ++i)
            {
                if (pass_enabled[i])
                {
                    active_passes[i] = true;
                    ++active_count;
                }
            }
        }

        if (active_count == 0)
        {
            return compiled_dag{};
        }

        // 4. Build DAG adjacency graph among active passes
        auto adjacency = vector<vector<uint32_t>>(_passes.size());
        auto in_degrees = vector<uint32_t>(_passes.size(), 0);

        for (size_t i = 0; i < _passes.size(); ++i)
        {
            if (!active_passes[i])
            {
                continue;
            }

            const auto consumer_idx = static_cast<uint32_t>(i);
            const auto& pass = _passes[i];

            auto add_dependency = [&](uint32_t producer_idx) {
                if (producer_idx == consumer_idx || !active_passes[producer_idx])
                {
                    return;
                }

                // Avoid duplicate edges
                auto& edges = adjacency[producer_idx];
                if (tempest::find(edges.begin(), edges.end(), consumer_idx) == edges.end())
                {
                    edges.push_back(consumer_idx);
                    ++in_degrees[consumer_idx];
                }
            };

            for (const auto& acc : pass.texture_accesses)
            {
                if (acc.type == access_type::read || acc.load_op == rhi::load_op::load || acc.texture.version > 0)
                {
                    const auto resolved = resolve_texture(acc.texture);
                    auto it = texture_producer.find(resolved);
                    if (it != texture_producer.end())
                    {
                        add_dependency(it->second);
                    }
                }
            }

            for (const auto& acc : pass.buffer_accesses)
            {
                if (acc.type == access_type::read || acc.buffer.version > 0)
                {
                    const auto resolved = resolve_buffer(acc.buffer);
                    auto it = buffer_producer.find(resolved);
                    if (it != buffer_producer.end())
                    {
                        add_dependency(it->second);
                    }
                }
            }
        }

        // 5. Kahn's Algorithm (Topological Sort)
        auto ready_queue = deque<uint32_t>{};
        for (size_t i = 0; i < _passes.size(); ++i)
        {
            if (active_passes[i] && in_degrees[i] == 0)
            {
                ready_queue.push_back(static_cast<uint32_t>(i));
            }
        }

        while (!ready_queue.empty())
        {
            const auto pass_idx = ready_queue.front();
            ready_queue.pop_front();

            result.sorted_pass_indices.push_back(pass_idx);

            for (const auto neighbor : adjacency[pass_idx])
            {
                --in_degrees[neighbor];
                if (in_degrees[neighbor] == 0)
                {
                    ready_queue.push_back(neighbor);
                }
            }
        }

        // Cycle check
        if (result.sorted_pass_indices.size() != active_count)
        {
            return unexpected(dag_compile_error::cycle_detected);
        }

        // 6. Resource Lifetime Interval Calculation
        auto update_texture_lifetime = [&](rg_texture_id tex, uint32_t order_idx) {
            const auto resolved = resolve_texture(tex);
            if (resolved.is_valid())
            {
                auto& lifetime = result.texture_lifetimes[resolved.id];
                if (lifetime.first_pass == resource_lifetime::invalid_pass)
                {
                    lifetime.first_pass = order_idx;
                    lifetime.last_pass = order_idx;
                }
                else
                {
                    lifetime.first_pass = tempest::min(lifetime.first_pass, order_idx);
                    lifetime.last_pass = tempest::max(lifetime.last_pass, order_idx);
                }
            }
        };

        auto update_buffer_lifetime = [&](rg_buffer_id buf, uint32_t order_idx) {
            const auto resolved = resolve_buffer(buf);
            if (resolved.is_valid())
            {
                auto& lifetime = result.buffer_lifetimes[resolved.id];
                if (lifetime.first_pass == resource_lifetime::invalid_pass)
                {
                    lifetime.first_pass = order_idx;
                    lifetime.last_pass = order_idx;
                }
                else
                {
                    lifetime.first_pass = tempest::min(lifetime.first_pass, order_idx);
                    lifetime.last_pass = tempest::max(lifetime.last_pass, order_idx);
                }
            }
        };

        for (size_t order = 0; order < result.sorted_pass_indices.size(); ++order)
        {
            const auto pass_idx = result.sorted_pass_indices[order];
            const auto order_idx = static_cast<uint32_t>(order);
            const auto& pass = _passes[pass_idx];

            for (const auto& acc : pass.texture_accesses)
            {
                update_texture_lifetime(acc.texture, order_idx);
            }

            for (const auto& out_tex : pass.texture_outputs)
            {
                update_texture_lifetime(out_tex, order_idx);
            }

            for (const auto& acc : pass.buffer_accesses)
            {
                update_buffer_lifetime(acc.buffer, order_idx);
            }

            for (const auto& out_buf : pass.buffer_outputs)
            {
                update_buffer_lifetime(out_buf, order_idx);
            }
        }

        return result;
    }
} // namespace tempest::render_graph
