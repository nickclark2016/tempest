project 'stb'
    kind 'Utility'
    language 'C'
    cdialect 'C11'

    targetdir '%{binaries}'
    objdir '%{intermidates}'

    files {
        'include/**.h',
    }

    IncludeDir['stb'] = '%{root}/dependencies/stb/include'