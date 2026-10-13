scoped.project('scene', function()
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
            'assets',
            'core',
            'ecs',
            'job',
            'logger',
            'serialization',
        }
    end)

    scoped.usage("scene:includedirs", function()
        externalincludedirs {
            'include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'scene:includedirs',
        }

        dependson {
            'scene',
        }

        links {
            'scene',
        }
    end)
end)

scoped.group('Tests', function()
    scoped.project('scene-tests', function()
        tags { 'non-gpu-test' }
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
            'scene',
        }

        scoped.filter({ 'system:linux' }, function()
            links { 'X11' }
            linkgroups 'On'
        end)

        warnings 'Extra'
    end)
end)
