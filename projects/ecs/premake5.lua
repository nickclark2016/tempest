scoped.group('Engine', function()
    scoped.project('ecs', function()
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
            '%{IncludeDir.core}',
            '%{IncludeDir.math}',
        }

        dependson {
            'core',
            'math',
        }

        links {
            'core',
            'glfw',
            'math',
            'tlsf',
        }

        IncludeDir['ecs'] = '%{root}/projects/ecs/include'

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
        scoped.project('ecs-tests', function()
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
                '%{IncludeDir.core}',
                '%{IncludeDir.math}',
            }

            externalincludedirs {
                '%{IncludeDir.gtest}',
            }
    
            dependson {
                'ecs',
                'googletest',
            }
    
            links {
                'core',
                'ecs',
                'glfw',
                'googletest',
                'math',
                'tlsf',
            }

            linkgroups 'On'

            scoped.filter({ 'system:linux' }, function()
                links { 'X11' }
            end)

            warnings 'Extra'
        end)
    end)
end)
