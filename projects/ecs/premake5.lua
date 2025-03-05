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
        }

        scoped.filter({
            'toolset:msc*'
        }, function()
            buildoptions {
                '/wd4324', -- 'structure was padded due to alignment specifier'
            }
        end)

        externalwarnings 'Off'
        warnings 'Extra'

        scoped.usage("PUBLIC", function()
            uses {
                'core',
                'math',
            }
        end)

        scoped.usage("INTERFACE", function()
            externalincludedirs {
                '%{root}/projects/ecs/include',
            }

            dependson {
                'ecs',
            }

            links {
                'ecs',
            }
        end)
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
            }

            uses {
                'ecs',
                'googletest',
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
