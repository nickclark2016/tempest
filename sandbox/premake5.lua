scoped.project('sandbox', function()
    kind 'ConsoleApp'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'
    debugdir '%{root}/projects/sandbox'

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
    }

    warnings 'Extra'

    scoped.filter({
        'system:not windows'
    }, function()
        linkgroups 'On'
    end)

    postbuildcommands {
        '{RMDIR} %{root}/sandbox/assets', -- Ensure a clean assets directory before building it
        '{MKDIR} %{root}/sandbox/assets',
        '{LINKDIR} %{root}/sandbox/assets/glTF-Sample-Assets ../../vendor/glTF-Sample-Assets',
        '{LINKDIR} %{root}/sandbox/assets/polyhaven ../../vendor/polyhaven',
        '{LINKDIR} %{root}/sandbox/assets/shaders ../../bin/%{cfg.buildcfg}/%{cfg.system}-%{cfg.toolset}/shaders',
    }

    if _OPTIONS['enable-aftermath'] then
        uses 'aftermath-runtime'
    end
end)
