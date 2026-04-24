scoped.project('event', function()
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

    externalwarnings 'Off'
    warnings 'Extra'

    scoped.usage("PUBLIC", function()
        uses { 'core' }
    end)

    scoped.usage("event:includedirs", function()
        externalincludedirs {
            '%{root}/engine/runtime/event/include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'event:includedirs',
        }

        dependson {
            'event',
        }

        links {
            'event',
        }
    end)
end)

scoped.group('Tests', function()
    scoped.project('event-tests', function()
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'tests/**.cpp',
        }

        uses {
            'event',
            'googletest',
        }

        externalwarnings 'Off'
        warnings 'Extra'
    end)
end)
