#include <tempest/render_system/shader_manager.hpp>

#include <tempest/files.hpp>
#include <filesystem>
#include <fstream>

namespace tempest::render_system
{
    shader_manager::shader_manager(rhi::device& dev, string_view shader_dir)
        : _device{&dev}, _shader_dir{shader_dir.data(), shader_dir.size()}
    {
    }

    shader_manager::~shader_manager()
    {
        release_all();
    }

    shader_manager::shader_manager(shader_manager&& other) noexcept
        : _device{other._device}, _shader_dir{tempest::move(other._shader_dir)},
          _bytecode_cache{tempest::move(other._bytecode_cache)},
          _graphics_pipelines{tempest::move(other._graphics_pipelines)},
          _compute_pipelines{tempest::move(other._compute_pipelines)}
    {
        other._device = nullptr;
    }

    shader_manager& shader_manager::operator=(shader_manager&& other) noexcept
    {
        if (this != &other)
        {
            release_all();
            _device = other._device;
            _shader_dir = tempest::move(other._shader_dir);
            _bytecode_cache = tempest::move(other._bytecode_cache);
            _graphics_pipelines = tempest::move(other._graphics_pipelines);
            _compute_pipelines = tempest::move(other._compute_pipelines);
            other._device = nullptr;
        }
        return *this;
    }

    auto shader_manager::load_shader_bytecode(string_view shader_filename) -> span<const byte>
    {
        const auto key = string{shader_filename.data(), shader_filename.size()};
        auto it = _bytecode_cache.find(key);
        if (it != _bytecode_cache.end())
        {
            return span<const byte>{it->second.data(), it->second.size()};
        }

        // Try potential paths
        const auto std_filename = std::string(shader_filename.data(), shader_filename.size());
        const auto search_paths = {
            std::filesystem::path(_shader_dir.c_str()) / std_filename,
            std::filesystem::path(_shader_dir.c_str()) / "rs" / std_filename,
            std::filesystem::path("shaders/rs") / std_filename,
            std::filesystem::path("shaders") / std_filename,
            std::filesystem::path("bin/Debug/windows-clang/shaders/rs") / std_filename,
            std::filesystem::path("bin/Debug/windows-clang/shaders") / std_filename,
            std::filesystem::path("bin/Debug/windows-msc-v145/shaders/rs") / std_filename,
            std::filesystem::path("bin/Debug/windows-msc-v145/shaders") / std_filename,
            std::filesystem::path(std_filename),
        };

        for (const auto& path : search_paths)
        {
            if (std::filesystem::exists(path))
            {
                auto path_str = path.string();
                auto bytes = core::read_bytes(string_view{path_str.c_str(), path_str.size()});
                if (!bytes.empty())
                {
                    _bytecode_cache[key] = tempest::move(bytes);
                    const auto& cached = _bytecode_cache[key];
                    return span<const byte>{cached.data(), cached.size()};
                }
            }
        }

        return {};
    }

    auto shader_manager::create_shader_module_desc(string_view shader_filename, rhi::shader_stage stage,
                                                   string_view entry_point) -> optional<rhi::shader_module_desc>
    {
        auto bytecode = load_shader_bytecode(shader_filename);
        if (bytecode.empty())
        {
            return nullopt;
        }

        return rhi::shader_module_desc{
            .stage = stage,
            .ir_code = bytecode,
            .entry_point = cstring_view{entry_point.data(), entry_point.size()},
            .specialization_constants = {},
        };
    }

    auto shader_manager::get_or_create_graphics_pipeline(string_view key, const rhi::graphics_pipeline_desc& desc)
        -> rhi::graphics_pipeline_handle
    {
        const auto key_str = string{key.data(), key.size()};
        auto it = _graphics_pipelines.find(key_str);
        if (it != _graphics_pipelines.end())
        {
            return it->second;
        }

        if (!_device)
        {
            return {};
        }

        auto handle = _device->create_graphics_pipeline(desc);
        if (handle.handle != 0)
        {
            _graphics_pipelines[key_str] = handle;
        }
        return handle;
    }

    auto shader_manager::get_or_create_compute_pipeline(string_view key, const rhi::compute_pipeline_desc& desc)
        -> rhi::compute_pipeline_handle
    {
        const auto key_str = string{key.data(), key.size()};
        auto it = _compute_pipelines.find(key_str);
        if (it != _compute_pipelines.end())
        {
            return it->second;
        }

        if (!_device)
        {
            return {};
        }

        auto handle = _device->create_compute_pipeline(desc);
        if (handle.handle != 0)
        {
            _compute_pipelines[key_str] = handle;
        }
        return handle;
    }

    void shader_manager::release_all()
    {
        if (_device)
        {
            for (auto& [_, pipe] : _graphics_pipelines)
            {
                if (pipe.handle != 0)
                {
                    _device->destroy_graphics_pipeline(pipe);
                }
            }
            _graphics_pipelines.clear();

            for (auto& [_, pipe] : _compute_pipelines)
            {
                if (pipe.handle != 0)
                {
                    _device->destroy_compute_pipeline(pipe);
                }
            }
            _compute_pipelines.clear();
        }
        _bytecode_cache.clear();
    }
} // namespace tempest::render_system
