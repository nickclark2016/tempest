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

        usage "INTERFACE"
            externalincludedirs {
                '%{root}/projects/tempest/include',
            }

            dependson {
                'tempest',
            }

            links {
                'tempest',
                -- List out the third party dependencies
                'imgui',
                'simdjson',
                'spdlog',
                'tlsf',
                'vk-bootstrap',
                'vma'
            }

            scoped.filter({ 'system:linux' }, function()
                links { 'X11' }
            end)

            scoped.filter({
                'toolset:msc*'
            }, function()
                buildoptions {
                    '/wd4324', -- 'structure was padded due to alignment specifier'
                }
            end)
