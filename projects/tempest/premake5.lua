scoped.group('Engine', function()
    scoped.project('tempest', function()
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

        scoped.usage("PUBLIC", function()
            uses {
                'assets',
                'core',
                'ecs',
                'graphics',
                'logger',
                'math',
            }
        end)

        scoped.usage("INTERFACE", function()
            externalincludedirs {
                '%{root}/projects/tempest/include',
            }

            dependson {
                'tempest',
            }

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
                'system:linux'
            }, function()
                links {
                    'pthread',
                    'X11',
                }
            end)
        end)

        scoped.filter({
            'toolset:msc*'
        }, function()
            buildoptions {
                '/wd4324', -- 'structure was padded due to alignment specifier'
            }
        end)
    end)
end)