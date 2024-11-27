project 'simdjson'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermidates}'

    files {
        'include/simdjson.h',
        'src/simdjson.cpp',
    }

    warnings 'Off'

    IncludeDir['simdjson'] = '%{root}/dependencies/simdjson/include'