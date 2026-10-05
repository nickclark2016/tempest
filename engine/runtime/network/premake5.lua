scoped.project('network', function()
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
            '/wd4324',
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
        uses { 'api', 'core' }
    end)

    scoped.usage("network:includedirs", function()
        externalincludedirs {
            'include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'network:includedirs',
        }

        dependson {
            'network',
        }

        links {
            'network',
        }

        scoped.filter({ 'system:windows' }, function()
            links { 'ws2_32' }
        end)
    end)
end)

scoped.group('Tests', function()
    scoped.project('network-tests', function()
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
        }

        scoped.filter({ 'system:windows' }, function()
            links { 'ws2_32' }
        end)

        externalwarnings 'Off'
        warnings 'Extra'
    end)
end)
