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
            '%{IncludeDir.math}',
        }

        externalincludedirs {
            '%{IncludeDir.glfw}',
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

        scoped.filter({
            'toolset:msc*'
        }, function()
            buildoptions {
                '/wd4324', -- 'structure was padded due to alignment specifier'
            }
        end)
    
        externalwarnings 'Off'
        warnings 'Extra'
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
                '%{IncludeDir.math}',
            }

            externalincludedirs {
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
            warnings 'Extra'
        end)
    end)
end)
