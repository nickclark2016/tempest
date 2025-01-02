local tempest = require '../tempest-config'

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

        tempest.applyTempestConfig()

        warnings 'Extra'

        postbuildcommands {
            '{RMDIR} %{root}/projects/editor/assets', -- Ensure a clean assets directory before building it
            '{MKDIR} %{root}/projects/editor/assets',
            '{LINKDIR} %{root}/projects/editor/assets/glTF-Sample-Assets ../../../vendor/glTF-Sample-Assets',
            '{LINKDIR} %{root}/projects/editor/assets/shaders ../../../bin/%{cfg.buildcfg}/%{cfg.system}/%{cfg.architecture}/shaders',
        }
    end)
end)
