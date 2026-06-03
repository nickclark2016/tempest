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
    }

    scoped.usage('PUBLIC', function()
        uses {
            'tempest',
            'imgui',
        }

        externalincludedirs {
            '%{root}/engine/editor/core/include',
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
                'TEMPEST_API',
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
end)