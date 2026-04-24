scoped.project('ecs', function()
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
        'toolset:msc*'
    }, function()
        buildoptions {
            '/wd4324', -- 'structure was padded due to alignment specifier'
        }
    end)

    externalwarnings 'Off'
    warnings 'Extra'

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
            'event',
            'math',
        }
    end)

    scoped.usage("ecs:includedirs", function()
        externalincludedirs {
            '%{root}/engine/runtime/ecs/include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'ecs:includedirs',
        }

        dependson {
            'ecs',
        }

        links {
            'ecs',
        }
    end)
end)

scoped.group('Tests', function()
    scoped.project('ecs-tests', function()
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
            'tempest',
            'googletest',
        }

        scoped.filter({ 'system:linux' }, function()
            links { 'X11' }
            linkgroups 'On'
        end)

        warnings 'Extra'
    end)
end)