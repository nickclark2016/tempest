scoped.project('sandbox', function()
    kind 'ConsoleApp'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'
    debugdir 'sandbox'

    files {
        'include/**.hpp',
        'src/**.cpp',
        'src/**.hpp',
    }

    includedirs {
        'include'
    }

    uses {
        'tempest',
        'rhi-vk',
    }

    warnings 'Extra'

    scoped.filter({
        'system:not windows'
    }, function()
        linkgroups 'On'
    end)

    postbuildcommands {
        '{RMDIR} assets', -- Ensure a clean assets directory before building it
        '{MKDIR} assets',
        '{LINKDIR} %{prj.basedir}/assets/glTF-Sample-Assets ../vendor/glTF-Sample-Assets',
        '{LINKDIR} %{prj.basedir}/assets/polyhaven ../vendor/polyhaven',
        '{LINKDIR} %{prj.basedir}/assets/shaders ../bin/%{cfg.buildcfg}/%{cfg.system}-%{cfg.toolset}/shaders',
    }

    if _OPTIONS['enable-aftermath'] then
        uses 'aftermath-runtime'
    end
end)
