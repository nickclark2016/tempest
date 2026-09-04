scoped.project('render-graph', function()
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

    scoped.usage("render-graph:includedirs", function()
        externalincludedirs {
            'include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'render-graph:includedirs',
        }

        dependson {
            'render-graph',
        }

        links {
            'render-graph',
        }
    end)

    scoped.usage("PUBLIC", function()
        uses {
            'api',
            'core',
            'profiler',
            'rhi-api',
        }
    end)
end)

scoped.group('Tests', function()
    scoped.project('render-graph-tests', function()
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
            'tempest',
        }
    end)
end)
