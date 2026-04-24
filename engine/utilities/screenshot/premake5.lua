scoped.project('screenshot', function()
    kind 'ConsoleApp'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'
    debugdir '%{root}/screenshot'

    files {
        'src/**.cpp',
        'src/**.hpp',
    }

    uses {
        'tempest',
        'stb',
    }

    warnings 'Extra'

    scoped.filter({
        'system:not windows'
    }, function()
        linkgroups 'On'
    end)

    postbuildcommands {
        '{RMDIR} %{!root}/engine/utilities/%{prj.name}/assets', -- Ensure a clean assets directory before building it
        '{MKDIR} %{!root}/engine/utilities/%{prj.name}/assets',
        '{LINKDIR} %{!root}/engine/utilities/%{prj.name}/assets/shaders %{!root}/bin/%{cfg.buildcfg}/%{cfg.system}-%{cfg.toolset}/shaders',
    }

    if _OPTIONS['enable-aftermath'] then
        uses 'aftermath-runtime'
    end
end)
