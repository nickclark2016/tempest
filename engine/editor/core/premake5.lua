scoped.project('editor-core', function()
    language 'C++'
    cppdialect 'C++20'
    kind 'SharedLib'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**.hpp',
        'src/**.cpp',
        'src/**.hpp',
        'shaders/**.slang',
    }

    prebuildcommands {
        '{MKDIR} "%{cfg.targetdir}/shaders/editor"',
    }

    uses {
        'tempest',
        'imgui',
        'rhi-api',
        'rhi-vk',
        'glfw',
    }

    scoped.usage('PUBLIC', function()
        uses {
            'tempest',
            'imgui',
            'rhi-api',
            'rhi-vk',
            'glfw',
        }

        externalincludedirs {
            'include',
        }
    end)

    scoped.usage('INTERFACE', function()
        links {
            'editor-core',
        }

        scoped.filter('system:windows', function()
            defines {
                'TEMPEST_EDITOR_API=__declspec(dllimport)',
            }
        end)

        scoped.filter('system:not windows', function()
            defines {
                'TEMPEST_EDITOR_API=',
            }
        end)
    end)

    scoped.filter('system:windows', function()
        defines {
            'TEMPEST_EDITOR_API=__declspec(dllexport)',
        }
    end)

    scoped.filter('system:not windows', function()
        defines {
            'TEMPEST_EDITOR_API=__attribute__((visibility("default")))'
        }
    end)

    scoped.filter({ 'files:shaders/**.slang' }, function()
        buildmessage 'Compiling %{file.relpath}'

        scoped.filter({
            'options:debug-shaders'
        }, function()
            buildcommands {
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -fvk-use-entrypoint-name -o %{!cfg.targetdir}/shaders/editor/%{file.basename}.vert.spv -entry VSMain -O0 -g3',
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -fvk-use-entrypoint-name -o %{!cfg.targetdir}/shaders/editor/%{file.basename}.frag.spv -entry FSMain -O0 -g3',
            }
        end)

        scoped.filter({
            'options:not debug-shaders'
        }, function()
            buildcommands {
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -fvk-use-entrypoint-name -o %{!cfg.targetdir}/shaders/editor/%{file.basename}.vert.spv -entry VSMain -O3',
                '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -fvk-use-entrypoint-name -o %{!cfg.targetdir}/shaders/editor/%{file.basename}.frag.spv -entry FSMain -O3',
            }
        end)

        buildoutputs {
            '%{!cfg.targetdir}/shaders/editor/%{file.basename}.vert.spv',
            '%{!cfg.targetdir}/shaders/editor/%{file.basename}.frag.spv'
        }
    end)
end)

scoped.group('Tests', function()
    scoped.project('editor-core-tests', function()
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
            'googletest',
            'editor-core',
            'rhi-vk',
            'tempest',
            'imgui',
            'glfw',
        }
    end)
end)