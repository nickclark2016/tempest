scoped.project('editor-entrypoint', function()
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'
    debugdir '%{root}/projects/editor/entrypoint'
    
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
        '{RMDIR} %{!root}/engine/editor/entrypoint/assets', -- Ensure a clean assets directory before building it
        '{MKDIR} %{!root}/engine/editor/entrypoint/assets',
        '{LINKDIR} %{!root}/engine/editor/entrypoint/assets/glTF-Sample-Assets ../../../../vendor/glTF-Sample-Assets',
        '{LINKDIR} %{!root}/engine/editor/entrypoint/assets/polyhaven ../../../../vendor/polyhaven',
        '{LINKDIR} %{!root}/engine/editor/entrypoint/assets/shaders ../../../../bin/%{cfg.buildcfg}/%{cfg.system}-%{cfg.toolset}/shaders',
    }

    if _OPTIONS['enable-aftermath'] then
        uses 'aftermath-runtime'
    end
end)