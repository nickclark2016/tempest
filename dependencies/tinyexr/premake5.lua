project 'tinyexr'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/tinyexr/tinyexr.h',
        'src/tinyexr.cc',
    }

    uses { 'miniz' }

    usage 'PUBLIC'
        defines {
            'TINYEXR_USE_MINIZ=1'
        }

    usage 'INTERFACE'
        externalincludedirs {
            '%{root}/dependencies/tinyexr/include',
        }

        dependson {
            'tinyexr',
        }

        links {
            'tinyexr',
        }