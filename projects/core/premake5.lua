scoped.group('Engine', function()
    scoped.project('core', function()
        kind 'StaticLib'
        language 'C++'
        cppdialect 'C++20'
    
        targetdir '%{binaries}'
        objdir '%{intermediates}'
    
        files {
            'include/**.hpp',
            'src/**.cpp',
            'src/**.hpp',
            'natvis/**.natvis',
        }
    
        includedirs {
            'include',
        }
    
        scoped.filter({
            'toolset:msc*'
        }, function()
            buildoptions {
                '/wd4324', -- 'structure was padded due to alignment specifier'
            }
        end)

        scoped.filter({
            'system:linux'
        }, function() {
            links {
                'pthread',
            }
        })
    
        externalwarnings 'Off'
        warnings 'Extra'

        uses { 'glfw', 'tlsf' }

        usage "PUBLIC"
            uses { 'math' }

        usage "INTERFACE"
            externalincludedirs {
                '%{root}/projects/core/include',
            }
    
            dependson {
                'core',
            }
    
            links {
                'core',
            }
    end)

    scoped.group('Tests', function()
        scoped.project('core-tests', function()
            kind 'ConsoleApp'
            language 'C++'
            cppdialect 'C++20'
        
            targetdir '%{binaries}'
            objdir '%{intermediates}'
        
            files {
                'tests/**.cpp',
            }

            uses {
                'core',
                'googletest',
            }
        
            externalwarnings 'Off'
            warnings 'Extra'
        end)
    end)
end)
