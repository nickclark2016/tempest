scoped.project('tasks', function()
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

    warnings 'Extra'
    
    scoped.filter({
        'options:shared-engine',
    }, function()
        defines {
            'TEMPEST_API_EXPORT'
        }
    end)

    scoped.usage('PUBLIC', function()
        uses {
            'api',
            'core',
        }
    end)

    scoped.usage('tasks:includedirs', function()
        externalincludedirs {
            '%{root}/engine/runtime/tasks/include',
        }
    end)

    scoped.usage('INTERFACE', function()
        uses {
            'tasks:includedirs',
        }

        dependson {
            'tasks',
        }

        links {
            'tasks',
        }
    end)
end)

scoped.group('Tests', function()
    scoped.project('tasks-tests', function()
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'tests/**.cpp',
        }

        includedirs {
            'include',
        }

        uses {
            'glfw',
            'googletest',
            'tasks',
            'tlsf',
        }
    end)
end)
