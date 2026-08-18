scoped.project('rhi-vk', function()
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**.hpp',
        'src/**.cpp',
        'src/**.hpp',
    }

    includedirs {
        'include',
    }

    scoped.filter({
        'options:shared-engine',
    }, function()
        defines {
            'TEMPEST_API_EXPORT'
        }
    end)

    uses {
        'vk-bootstrap',
        'vulkan',
    }

    scoped.usage("rhi-vk:includedirs", function()
        externalincludedirs {
            'include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'rhi-vk:includedirs'
        }

        dependson {
            'rhi-vk',
        }

        links {
            'rhi-vk',
        }
    end)

    scoped.usage("PUBLIC", function()
        uses {
            'rhi-api',
            'vk-bootstrap',
            'vma',
        }
    end)
end)

scoped.group('Tests', function()
    scoped.project('rhi-vk-tests', function()
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'tests/**.cpp',
            'tests/**.hpp',
            'tests/shaders/**.slang',
        }

        includedirs {
            '%{cfg.objdir}/shaders',
        }

        scoped.filter({ 'files:tests/shaders/test_compute.slang' }, function()
            buildmessage 'Compiling %{file.relpath}'
            buildcommands {
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -entry CSMain -stage compute -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name test_compute_spv -o %{!cfg.objdir}/shaders/test_compute.comp.h',
                'touch %{!cfg.objdir}/shaders/test_compute.comp.h',
            }
            buildoutputs {
                '%{!cfg.objdir}/shaders/test_compute.comp.h',
            }
        end)

        scoped.filter({ 'files:tests/shaders/test_raster.slang' }, function()
            buildmessage 'Compiling %{file.relpath}'
            buildcommands {
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -entry VSMain -stage vertex -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name test_raster_vs_spv -o %{!cfg.objdir}/shaders/test_raster.vert.h',
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -entry FSMain -stage fragment -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name test_raster_fs_spv -o %{!cfg.objdir}/shaders/test_raster.frag.h',
                'touch %{!cfg.objdir}/shaders/test_raster.vert.h',
                'touch %{!cfg.objdir}/shaders/test_raster.frag.h',
            }
            buildoutputs {
                '%{!cfg.objdir}/shaders/test_raster.vert.h',
                '%{!cfg.objdir}/shaders/test_raster.frag.h',
            }
        end)

        scoped.filter({ 'files:tests/shaders/test_bindless.slang' }, function()
            buildmessage 'Compiling %{file.relpath}'
            buildcommands {
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -entry compute_sample -stage compute -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name test_bindless_sample_spv -o %{!cfg.objdir}/shaders/test_bindless_sample.comp.h',
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -entry compute_storage_write -stage compute -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name test_bindless_storage_spv -o %{!cfg.objdir}/shaders/test_bindless_storage.comp.h',
                'touch %{!cfg.objdir}/shaders/test_bindless_sample.comp.h',
                'touch %{!cfg.objdir}/shaders/test_bindless_storage.comp.h',
            }
            buildoutputs {
                '%{!cfg.objdir}/shaders/test_bindless_sample.comp.h',
                '%{!cfg.objdir}/shaders/test_bindless_storage.comp.h',
            }
        end)

        uses {
            'googletest',
            'rhi-vk',
            'tempest',
            'glfw',
        }
    end)
end)
