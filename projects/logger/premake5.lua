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

        defines {
            '_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS',
        }

        scoped.filter({
            'toolset:msc*'
        }, function()
            buildoptions {
                '/wd4324', -- 'structure was padded due to alignment specifier'
            }
        end)

        externalwarnings 'Off'
        warnings 'Extra'

        uses { 'spdlog' }

        scoped.usage("INTERFACE", function()
            externalincludedirs {
                '%{root}/projects/logger/include',
            }

            dependson {
                'logger',
            }

            links {
                'logger',
            }
        end)
    end)
end)
