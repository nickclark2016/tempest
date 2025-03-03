group 'Engine'
    project 'tempest'
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

        externalwarnings 'Off'
        warnings 'Extra'

        includedirs {
            'include'
        }

        usage "PUBLIC"
            uses {
                'assets',
                'core',
                'ecs',
                'graphics',
                'logger',
                'math',
            }

        usage "INTERFACE"
            externalincludedirs {
                '%{root}/projects/tempest/include',
            }

            dependson {
                'tempest',
            }

            scoped.filter({
                'system:linux'
            }, function()
                links {
                    'pthread',
                    'X11',
                }
            end)

            links {
                'tempest',
                -- List out the third party dependencies
                'glfw',
                'imgui',
                'simdjson',
                'spdlog',
                'tlsf',
                'vk-bootstrap',
                'vma'
            }

            scoped.filter({
                'toolset:msc*'
            }, function()
                buildoptions {
                    '/wd4324', -- 'structure was padded due to alignment specifier'
                }
            end)
