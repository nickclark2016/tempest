group 'Engine'
    project 'ecs'
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

group 'Tests'
    project 'ecs-tests'
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
