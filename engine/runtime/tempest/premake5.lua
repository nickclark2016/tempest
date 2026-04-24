scoped.project('tempest', function()
    scoped.filter({
        'options:shared-engine',
    }, function()
        kind 'SharedLib'
    end)

    scoped.filter({
        'not options:shared-engine',
    }, function()
        kind 'StaticLib'
    end)

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

        scoped.filter({
        'toolset:clang',
        'system:windows',
        'action:not vs*',
        'configurations:Debug',
        'kind:ConsoleApp or SharedLib or WindowedApp'
    }, function()
        linkoptions {
            '-dll_dbg',
            '-Xlinker /NODEFAULTLIB:libcmt'
        }

        links {
            "libcmtd"
        }
    end)

    scoped.filter({
        'toolset:clang',
        'system:windows',
        'action:not vs*',
        'configurations:RelWithDebugInfo or Release',
        'kind:ConsoleApp or SharedLib or WindowedApp'
    }, function()
        linkoptions {
            '-dll',
            '-Xlinker /NODEFAULTLIB:libcmt'
        }

        links {
            "libcmtd"
        }
    end)

    scoped.usage("PRIVATE", function()
        uses {
            'api',
            'assets',
            'core',
            'ecs',
            'event',
            'graphics',
            'logger',
            'math',
            'rhi-api',
            'serialization',
            'tasks',
        }
    end)

    scoped.filter({
        'options:shared-engine',
    }, function()
        defines {
            'TEMPEST_API_EXPORT'
        }
    end)

    scoped.filter({
        'toolset:clang',
        'system:windows',
    }, function()
        linkoptions {
            '-Wl,/implib:%{binaries}/tempest.lib'
        }
    end)

    scoped.filter({
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

    scoped.usage("INTERFACE", function()
        externalincludedirs {
            '%{root}/engine/runtime/tempest/include',
        }

        uses {
            'api:includedirs',
            'assets:includedirs',
            'core:defines',
            'core:includedirs',
            'ecs:includedirs',
            'event:includedirs',
            'graphics:includedirs',
            'logger:includedirs',
            'math:includedirs',
            'rhi-api:includedirs',
            'serialization:includedirs',
            'tasks:includedirs',
        }

        dependson {
            'tempest',
        }

        links {
            'tempest',
        }

        scoped.filter({
            'kind:StaticLib',
        }, function()
            links {
                -- Temporary until we are a DLL and can export symbols properly
                'assets',
                'core',
                'ecs',
                'event',
                'graphics',
                'logger',
                'math',
                'rhi-api',
                'serialization',
                'tasks',
                -- Dependency libraries
                'glfw',
                'miniz',
                'simdjson',
                'tinyexr',
                'tlsf',
                'vk-bootstrap',
                'vma',
           }
        end)

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
