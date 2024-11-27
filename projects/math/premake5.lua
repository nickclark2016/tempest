group 'Engine'
    project 'math'
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
    }

    IncludeDir['math'] = '%{root}/projects/math/include'

group 'Tests'
    project 'math-tests'
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
            'math',
            'googletest',
        }

        links {
            'math',
            'googletest',
        }