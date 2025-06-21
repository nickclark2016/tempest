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
            'rhi-api',
        }

        links {
            'rhi-vk',
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
            'miniz',
            'simdjson',
            'spdlog',
            'tinyexr',
            'tlsf',
            'vk-bootstrap',
            'vma'
        }

        if _OPTIONS['enable-aftermath'] then
            libdirs('%{root}/dependencies/aftermath/lib/x64')
            links('GFSDK_Aftermath_Lib.x64')
        end

        scoped.filter({
            'system:windows'
        }, function()
            links {
                'kernel32',
                'user32',
            }
        end)

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
