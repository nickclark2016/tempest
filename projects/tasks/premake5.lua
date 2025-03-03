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

        usage 'PUBLIC'
            uses { 'core' }

        usage 'INTERFACE'
            externalincludedirs {
                '%{root}/projects/tasks/include',
            }

        usage "*"
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
                'googletest',
                'tasks',
            }
        end)
    end)
end)