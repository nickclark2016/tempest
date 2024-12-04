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
            '%{IncludeDir.glfw}',
            '%{IncludeDir.math}',
            '%{IncludeDir.tlsf}',
        }
    
        links {
            'glfw',
            'math',
            'tlsf',
        }
    
        dependson {
            'glfw',
            'math',
            'tlsf',
        }
    
        IncludeDir['core'] = '%{root}/projects/core/include'
    
        externalwarnings 'Off'
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
        
            includedirs {
                'include',
                '%{IncludeDir.gtest}',
            }
        
            dependson {
                'core',
                'googletest',
            }
        
            links {
                'core',
                'googletest',
            }
        
            externalwarnings 'Off'
        end)
    end)
end)
