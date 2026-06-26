project 'simdjson'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/simdjson.h',
        'src/simdjson.cpp',
    }

    warnings 'Off'

    usage "INTERFACE"
        externalincludedirs {
            'include',
        }

        dependson {
            'simdjson',
        }

        links {
            'simdjson',
        }