scoped.project('serialization', function(prj)
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

    includedirs {
        'include',
    }

    scoped.usage('PUBLIC', function()
        uses {
            'core',
            'logger',
            'math',
        }
    end)

    scoped.usage('serialization:includedirs', function()
        externalincludedirs {
            '%{root}/engine/runtime/serialization/include',
        }
    end)

    scoped.usage('INTERFACE', function()
        uses {
            'serialization:includedirs',
        }

        dependson {
            'serialization',
        }

        links {
            'serialization',
        }
    end)
end)

scoped.group('Tests', function()
    scoped.project('serialization-tests', function()
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'
    
        targetdir '%{binaries}'
        objdir '%{intermediates}'
    
        files {
            'tests/**.cpp',
        }

        uses {
            'googletest',
            'serialization'
        }
    
        externalwarnings 'Off'
        warnings 'Extra'
    end)
end)