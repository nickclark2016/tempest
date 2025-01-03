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

        scoped.filter({
            'toolset:msc*',
        }, function()
            files {
                'asm/msc/**.s',
            }

            scoped.filter({
                'files:**.s',
            }, function()
                buildaction "Masm"
            end)
        end)

        scoped.filter({
            'toolset:clang',
        }, function()
            files {
                'asm/clang/**.s',
            }
        end)

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

        scoped.filter({
            'toolset:msc*'
        }, function()
            buildoptions {
                '/wd4324', -- 'structure was padded due to alignment specifier'
            }
        end)

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
