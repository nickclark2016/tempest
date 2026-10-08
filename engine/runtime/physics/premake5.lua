include 'shim/premake5.lua'

scoped.project('physics', function()
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


    scoped.usage("PRIVATE", function()
        uses {
            'jolt-shim',
            'core',
            'ecs',
            'logger',
            'job',
            'network:includedirs',
        }
    end)

    scoped.usage("physics:includedirs", function()
        externalincludedirs {
            'include',
        }
        uses {
            'jolt-shim:includedirs',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'physics:includedirs',
            'jolt-shim',
            'core:includedirs',
            'ecs:includedirs',
            'network:includedirs',
        }

        dependson {
            'physics',
        }

        links {
            'physics',
        }
    end)
end)

scoped.group('Tests', function()
    scoped.project('physics-tests', function()
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
            'jolt-shim',
            'googletest',
        }

        scoped.filter({ 'system:linux' }, function()
            links { 'pthread', 'dl', 'atomic' }
        end)

        warnings 'Extra'
    end)
end)
