project 'spdlog'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**',
        'src/**',
    }

    defines {
        'SPDLOG_COMPILED_LIB',
    }

    includedirs {
        './include',
    }

    filter { 'system:windows' }
        staticruntime 'Off'
        defines {
            '_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS',
        }
    
    filter {}
    
    warnings 'Off'

    IncludeDir['spdlog'] = '%{root}/dependencies/spdlog/include'
    