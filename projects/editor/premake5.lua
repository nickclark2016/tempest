scoped.group('Editor', function()
    scoped.project('editor', function()
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'
        debugdir '%{root}/projects/editor'
    
        files {
            'include/**.hpp',
            'src/**.cpp',
            'src/**.hpp',
        }

        includedirs {
            'include'
        }

        uses {
            'tempest'
        }

        warnings 'Extra'

        scoped.filter({
            'system:not windows'
        }, function()
            linkgroups 'On'
        end)

        postbuildcommands {
            '{RMDIR} %{root}/projects/editor/assets', -- Ensure a clean assets directory before building it
            '{MKDIR} %{root}/projects/editor/assets',
            '{LINKDIR} %{root}/projects/editor/assets/glTF-Sample-Assets ../../../vendor/glTF-Sample-Assets',
            '{LINKDIR} %{root}/projects/editor/assets/polyhaven ../../../vendor/polyhaven',
            '{LINKDIR} %{root}/projects/editor/assets/shaders ../../../bin/%{cfg.buildcfg}/%{cfg.system}-%{cfg.toolset}/shaders',
        }
    end)
end)
