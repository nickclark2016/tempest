scoped.group('Engine', function()
    scoped.project('ecs', function()
        kind 'StaticLib'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermidates}'

        files {
            'include/**.hpp',
            'src/**.cpp',
            'src/**.hpp',
        }

        includedirs {
            'include',
            '%{IncludeDir.core}',
            '%{IncludeDir.math}',
        }

        dependson {
            'core',
            'math',
        }

        links {
            'core',
            'glfw',
            'math',
            'tlsf',
        }

        IncludeDir['ecs'] = '%{root}/projects/ecs/include'

        externalwarnings 'Off'
    end)
end)

scoped.group('Tests', function()
    scoped.project('ecs-tests', function()
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermidates}'

        files {
            'tests/**.cpp',
        }

        includedirs {
            'include',
            '%{IncludeDir.core}',
            '%{IncludeDir.gtest}',
            '%{IncludeDir.math}',
        }

        dependson {
            'ecs',
            'googletest',
        }

        links {
            'core',
            'ecs',
            'glfw',
            'googletest',
            'math',
            'tlsf',
        }
    end)
end)
