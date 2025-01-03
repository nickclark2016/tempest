local tempest = require '../tempest-config'

group 'Engine'
    project 'tempest'
        kind 'StaticLib'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'include/**.hpp',
            'src/**.cpp',
            'src/**.hpp',
        }

        externalwarnings 'Off'
        warnings 'Extra'

        tempest.applyTempestInternalConfig()

        IncludeDir['tempest'] = '%{root}/projects/tempest/include'
        
