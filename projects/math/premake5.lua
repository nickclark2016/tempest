scoped.group('Engine', function()
    scoped.project('math', function()
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

        warnings 'Extra'

        scoped.usage('INTERFACE', function()
            externalincludedirs {
                '%{root}/projects/math/include',
            }

            dependson {
                'math',
            }

            links {
                'math',
            }
        end)
    end)

    scoped.group('Tests', function()
        scoped.project('math-tests', function()
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
                'math',
            }

            warnings 'Extra'
        end)
    end)
end)
