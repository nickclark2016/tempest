scoped.group('Engine', function()
    scoped.group('RHI', function()
        scoped.project('rhi-vk', function()
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

            uses {
                'glfw',
                'logger',
                'rhi-api',
                'vk-bootstrap',
            }

            warnings 'Extra'

            scoped.usage('INTERFACE', function()
                links {
                    'rhi-vk'
                }

                dependson {
                    'rhi-vk'
                }
            end)
        end)
    end)
end)