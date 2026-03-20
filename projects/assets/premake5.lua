scoped.project('assets', function()
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

    uses {
        'stb',
        'simdjson',
        'tinyexr',
    }

    scoped.usage("PUBLIC", function()
        uses {
            'core',
            'ecs',
            'logger',
            'math',
            'serialization',
        }
    end)

    scoped.usage("INTERFACE", function()
        externalincludedirs {
            '%{root}/projects/assets/include',
        }

        dependson {
            'assets',
        }

        links {
            'assets',
            'miniz',
            'simdjson',
            'tinyexr',
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
    scoped.project('assets-tests', function()
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'tests/**.cpp',
        }

        uses {
            'googletest',
            'assets',
            'serialization',
        }

        externalwarnings 'Off'
        warnings 'Extra'
    end)
end)
