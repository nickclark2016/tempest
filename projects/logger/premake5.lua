scoped.group('Engine', function()
    scoped.project('logger', function()
        kind 'StaticLib'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'include/**.hpp',
            'src/**.cpp',
            'src/**.hpp',
        }

        includedirs {
            'include',
            '%{IncludeDir.spdlog}',
        }

        links {
            'spdlog',
        }

        dependson {
            'spdlog',
        }

        defines {
            '_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS',
        }

        externalwarnings 'Off'
        warnings 'Extra'

        IncludeDir['logger'] = '%{root}/projects/logger/include'
    end)
end)
