project 'tlsf'
    kind 'StaticLib'
    language 'C'
    cdialect 'C11'

    targetdir '%{binaries}'
    objdir '%{intermidates}'

    files {
        'include/tlsf/tlsf.h',
        'src/tlsf.c',
    }

    includedirs {
        'include/tlsf',
    }

    IncludeDir['tlsf'] = '%{root}/dependencies/tlsf/include'