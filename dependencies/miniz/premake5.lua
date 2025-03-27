project 'miniz'
    kind 'StaticLib'
    language 'C'
    cdialect 'C11'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/miniz/miniz.h',
        'src/miniz.c',
    }

    usage 'INTERFACE'
        externalincludedirs {
            '%{root}/dependencies/miniz/include',
        }

        dependson {
            'miniz',
        }

        links {
            'miniz',
        }
