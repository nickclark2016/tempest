scoped.project('render-system', function()
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**.hpp',
        'src/**.cpp',
        'src/**.hpp',
        -- Shaders
        'shaders/**.slang',
    }

    includedirs {
        'include',
    }

    prebuildcommands {
        '{MKDIR} "%{cfg.targetdir}/shaders/rs"',
    }

    uses { 'vulkan', 'rhi-vk', 'render-graph' }

    scoped.filter({
        'options:shared-engine',
    }, function()
        defines {
            'TEMPEST_API_EXPORT'
        }
    end)

    scoped.usage("PUBLIC", function()
        uses {
            'api',
            'core',
            'ecs',
            'logger',
            'assets',
            'rhi-api',
            'render-graph',
        }

        links {
            'rhi-vk',
            'render-graph',
        }
    end)

    scoped.usage("render-system:includedirs", function()
        externalincludedirs {
            'include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'render-system:includedirs',
        }

        dependson {
            'render-system',
        }

        links {
            'render-system',
            'render-graph',
            'rhi-vk',
        }
    end)

    scoped.filter({ 'files:shaders/raster/**.slang' }, function()
        buildmessage 'Compiling %{file.relpath}'

        scoped.filter({
            'options:debug-shaders'
        }, function()
            buildcommands {
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -fvk-use-entrypoint-name -o %{!cfg.targetdir}/shaders/rs/%{file.basename}.vert.spv -entry VSMain -O0 -g3',
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -fvk-use-entrypoint-name -o %{!cfg.targetdir}/shaders/rs/%{file.basename}.frag.spv -entry FSMain -O0 -g3',
            }
        end)

        scoped.filter({
            'options:not debug-shaders'
        }, function()
            buildcommands {
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -fvk-use-entrypoint-name -o %{!cfg.targetdir}/shaders/rs/%{file.basename}.vert.spv -entry VSMain -O3',
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -fvk-use-entrypoint-name -o %{!cfg.targetdir}/shaders/rs/%{file.basename}.frag.spv -entry FSMain -O3',
            }
        end)

        buildoutputs {
            '%{!cfg.targetdir}/shaders/rs/%{file.basename}.vert.spv',
            '%{!cfg.targetdir}/shaders/rs/%{file.basename}.frag.spv'
        }

        buildinputs {
            'shaders/common/**.slang',
        }
    end)

    scoped.filter({ 'files:shaders/compute/**.slang' }, function()
        buildmessage 'Compiling %{file.relpath}'

        scoped.filter({
            'options:debug-shaders'
        }, function()
            buildcommands {
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -fvk-use-entrypoint-name -o %{!cfg.targetdir}/shaders/rs/%{file.basename}.comp.spv -entry CSMain -O0 -g3',
            }
        end)

        scoped.filter({
            'options:not debug-shaders'
        }, function()
            buildcommands {
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -fvk-use-entrypoint-name -o %{!cfg.targetdir}/shaders/rs/%{file.basename}.comp.spv -entry CSMain -O3',
            }
        end)
        
        buildoutputs {
            '%{!cfg.targetdir}/shaders/rs/%{file.basename}.comp.spv',
        }

        buildinputs {
            'shaders/common/**.slang',
        }
    end)

    scoped.filter({
        'toolset:msc*'
    }, function()
        buildoptions {
            '/wd4324', -- 'structure was padded due to alignment specifier'
        }
    end)

    externalwarnings 'Off'
    warnings 'Extra'
end)

scoped.group('Tests', function()
    scoped.project('render-system-tests', function()
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'
    
        targetdir '%{binaries}'
        objdir '%{intermediates}'
    
        files {
            'tests/**.cpp',
            'tests/**.hpp',
        }

        includedirs {
            'include',
        }

        uses {
            'tempest',
            'googletest',
        }

        linkgroups 'On'
    
        externalwarnings 'Off'
        warnings 'Extra'
    end)
end)
