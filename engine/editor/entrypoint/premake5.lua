scoped.project('editor-entrypoint', function()
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'
    debugdir 'engine/editor/entrypoint'
    
    scoped.filter({
        'system:not windows'
    }, function()
        kind 'ConsoleApp'
    end)
    
    scoped.filter({
        'system:windows'
    }, function()
        kind 'WindowedApp'

        links {
            'shell32'
        }
    end)

    files {
        'src/**.cpp',
        'src/**.hpp',
    }

    uses {
        'tempest',
        'editor-core',
    }

    dependson {
        'game-editor',
        'game-runtime',
    }

    scoped.filter({
        'system:not windows'
    }, function()
        linkgroups 'On'
    end)

    postbuildcommands {
        '{RMDIR} %[%{!prj.basedir}/assets]', -- Ensure a clean assets directory before building it
        '{MKDIR} %[%{!prj.basedir}/assets]',
        '{LINKDIR} %[%{!prj.basedir}/assets/glTF-Sample-Assets] ../../../../vendor/glTF-Sample-Assets',
        '{LINKDIR} %[%{!prj.basedir}/assets/polyhaven] ../../../../vendor/polyhaven',
        '{LINKDIR} %[%{!prj.basedir}/assets/shaders] ../../../../bin/%{cfg.buildcfg}/%{cfg.system}-%{cfg.toolset}/shaders',
    }

    if _OPTIONS['enable-aftermath'] then
        uses 'aftermath-runtime'
    end
end)