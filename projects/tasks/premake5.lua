scoped.group('Engine', function()
    scoped.project('tasks', function()
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

        warnings 'Extra'

        scoped.usage('PUBLIC', function()
            uses {
                'core',
            }
        end)

        scoped.usage('INTERFACE', function()
            externalincludedirs {
                '%{root}/projects/tasks/include',
            }

            dependson {
                'tasks',
            }

            links {
                'tasks',
            }
        end)
    end)

    scoped.group('Tests', function()
        scoped.project('tasks-tests', function()
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
                'glfw',
                'googletest',
                'tasks',
                'tlsf',
            }
        end)
    end)
end)