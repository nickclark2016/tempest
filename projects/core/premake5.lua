group 'Engine'
    project 'core'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermidates}'

    files {
        'include/**.hpp',
        'src/**.cpp',
        'src/**.hpp',
        'natvis/**.natvis',
    }

    includedirs {
        'include',
        '%{IncludeDir.glfw}',
        '%{IncludeDir.math}',
        '%{IncludeDir.tlsf}',
    }

    links {
        'glfw',
        'math',
        'tlsf',
    }

    dependson {
        'glfw',
        'math',
        'tlsf',
    }

    IncludeDir['core'] = '%{root}/projects/core/include'

    externalwarnings 'Off'


group 'Tests'
    project 'core-tests'
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
            '%{IncludeDir.gtest}',
        }

        dependson {
            'core',
            'googletest',
        }

        links {
            'core',
            'googletest',
        }

        externalwarnings 'Off'