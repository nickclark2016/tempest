scoped.group('Engine', function()
    scoped.project('assets', function()
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

        uses {
            'stb',
            'simdjson'
        }

        usage "PUBLIC"
            uses {
                'core',
                'ecs',
                'logger',
                'math',
            }
        
        usage "INTERFACE"
            externalincludedirs {
                '%{root}/projects/assets/include',
            }

            dependson {
                'assets',
            }

            links {
                'assets',
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
    end)
end)
