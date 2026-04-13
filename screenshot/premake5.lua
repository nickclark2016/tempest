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

    includedirs {
        '%{root}/dependencies/stb/include',
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
        '{RMDIR} %{!root}/%{prj.name}/assets', -- Ensure a clean assets directory before building it
        '{MKDIR} %{!root}/%{prj.name}/assets',
        '{LINKDIR} %{!root}/%{prj.name}/assets/glTF-Sample-Assets %{!root}/vendor/glTF-Sample-Assets',
        '{LINKDIR} %{!root}/%{prj.name}/assets/polyhaven %{!root}/vendor/polyhaven',
        '{LINKDIR} %{!root}/%{prj.name}/assets/shaders %{!root}/bin/%{cfg.buildcfg}/%{cfg.system}-%{cfg.toolset}/shaders',
    }

    if _OPTIONS['enable-aftermath'] then
        uses 'aftermath-runtime'
    end
end)
