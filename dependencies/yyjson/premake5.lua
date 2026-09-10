project 'yyjson'
    kind 'StaticLib'
    language 'C'
    cdialect 'C11'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/yyjson.h',
        'src/yyjson.c',
    }

    includedirs {
        'include',
    }

    warnings 'Off'

    usage 'INTERFACE'
        externalincludedirs {
            'include',
        }

        dependson {
            'yyjson',
        }

        links {
            'yyjson',
        }
