scoped.group('Engine', function()
    scoped.group('RHI', function()
        scoped.project('rhi-api', function()
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

            scoped.usage('PUBLIC', function()
                uses {
                    'core'
                }
            end)

            scoped.usage('INTERFACE', function()
                externalincludedirs {
                    '%{root}/projects/rhi/api/include',
                }

                dependson {
                    'rhi-api',
                }

                links {
                    'rhi-api',
                }
            end)
        end)
    end)
end)