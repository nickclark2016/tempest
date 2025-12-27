scoped.project('editor-entrypoint', function()
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'
    
    scoped.filter({
        'system:not windows'
    }, function()
        kind 'ConsoleApp'
    end)
    
    scoped.filter({
        'system:windows'
    }, function()
        kind 'WindowedApp'
    end)

    files {
        'src/**.cpp',
        'src/**.hpp',
    }

    uses {
        'tempest',
        'imgui',
    }

    scoped.filter({
        'system:not windows'
    }, function()
        linkgroups 'On'
    end)

    postbuildcommands {
        '{RMDIR} %{!root}/projects/editor/entrypoint/assets', -- Ensure a clean assets directory before building it
        '{MKDIR} %{!root}/projects/editor/entrypoint/assets',
        '{LINKDIR} %{!root}/projects/editor/entrypoint/assets/glTF-Sample-Assets ../../../../vendor/glTF-Sample-Assets',
        '{LINKDIR} %{!root}/projects/editor/entrypoint/assets/polyhaven ../../../../vendor/polyhaven',
        '{LINKDIR} %{!root}/projects/editor/entrypoint/assets/shaders ../../../../bin/%{cfg.buildcfg}/%{cfg.system}-%{cfg.toolset}/shaders',
    }

    if _OPTIONS['enable-aftermath'] then
        uses 'aftermath-runtime'
    end
end)