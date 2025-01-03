scoped.group('Engine', function()
    scoped.project('math', function()
        kind 'StaticLib'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'asm/**.s',
            'include/**.hpp',
            'src/**.cpp',
            'src/**.hpp',
        }

        includedirs {
            'include',
        }

        scoped.filter({
            'toolset:clang',
            'files:**.s',
        }, function()
            buildoptions {
                '-masm=intel',
            }
        end)

        IncludeDir['math'] = '%{root}/projects/math/include'

        warnings 'Extra'
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

            warnings 'Extra'
        end)
    end)
end)
