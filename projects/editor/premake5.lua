local tempest = require '../tempest-config'

group 'Engine'
    project 'editor'
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

        