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
        }
        
        externalincludedirs {
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

        IncludeDir['logger'] = '%{root}/projects/logger/include'

        scoped.filter({
            'toolset:msc*'
        }, function()
            buildoptions {
                '/wd4324', -- 'structure was padded due to alignment specifier'
            }
        end)

        externalwarnings 'Off'
        warnings 'Extra'
    end)
end)
