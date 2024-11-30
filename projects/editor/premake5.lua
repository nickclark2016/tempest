local tempest = require '../tempest-config'

scoped.group('Editor', function()
    scoped.project('editor', function()
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermidates}'
        debugdir '%{root}/sandbox'
    
        files {
            'include/**.hpp',
            'src/**.cpp',
            'src/**.hpp',
        }

        includedirs {
            'include'
        }

        tempest.applyTempestConfig()
    end)
end)
        