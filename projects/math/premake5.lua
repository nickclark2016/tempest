scoped.group('Engine', function()
    scoped.project('math', function()
        kind 'StaticLib'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermidates}'

        files {
            'include/**.hpp',
            'src/**.cpp',
            'src/**.hpp',
        }

        includedirs {
            'include',
        }

        IncludeDir['math'] = '%{root}/projects/math/include'
    end)

    scoped.group('Tests', function()
        scoped.project('math-tests', function()
            kind 'ConsoleApp'
            language 'C++'
            cppdialect 'C++20'
    
            targetdir '%{binaries}'
            objdir '%{intermidates}'
    
            files {
                'tests/**.cpp',
            }
    
            includedirs {
                'include',
                '%{IncludeDir.gtest}',
            }
    
            dependson {
                'math',
                'googletest',
            }
    
            links {
                'math',
                'googletest',
            }
        end)
    end)
end)
