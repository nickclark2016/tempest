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
            'options:enable-lto'
        }, function()
            linktimeoptimization 'On'
        end)

        scoped.filter({
            'options:enable-lto',
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

local gcc = premake.tools.gcc
local gcc_whole_archive = gcc.wholearchive

if os.target() == 'windows' then
    local msc = premake.tools.msc
    local msc_whole_archive = msc.wholearchive

    gcc.wholearchive = function(cfg)
        local archives = premake.config.getwholearchive(cfg)
        archives = table.translate(archives, function(archive)
            return '-Wl,/WHOLEARCHIVE:' .. archive
        end)
        return archives
    end
end

newoption {
    trigger = 'enable-lto',
    description = 'Enable Link Time Optimization',
    category = 'Tempest Engine',
}
