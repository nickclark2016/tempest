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
            'event',
            'graphics',
            'logger',
            'math',
            'rhi-api',
            'serialization',
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
            'tempest'
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

        links {
            'rhi-vk',
        }

        scoped.filter({
            'configurations:Release',
        }, function()
            linktimeoptimization 'On'
        end)

        scoped.filter({
            'configurations:Release',
            'toolset:clang*',
        }, function()
            linker 'lld'
            
            wholearchive {
                'assets',
                'core',
                'ecs',
                'event',
                'graphics',
                'logger',
                'math',
                'rhi-api',
                'rhi-vk',
                'serialization',

                'miniz',
                'simdjson',
                'tinyexr',
                'tlsf',
                'vk-bootstrap',
                'vma',
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
