scoped.group('RHI', function()
    scoped.project('rhi-vk', function()
        kind 'StaticLib'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'include/tempest/vk/*.hpp',
            'src/*.cpp',
            'src/*.hpp',
        }

        includedirs {
            'include',
        }

        uses {
            'glfw',
            'logger',
            'rhi-api',
            'vk-bootstrap',
            'vma',
        }

        if _OPTIONS['debug-shaders'] then
            defines {
                'TEMPEST_DEBUG_SHADERS',
            }
        end

        if _OPTIONS['enable-aftermath'] then
            uses 'aftermath'
            defines {
                'TEMPEST_ENABLE_AFTERMATH',
            }

            files {
                'include/tempest/vk/aftermath/*.hpp',
                'src/aftermath/*.cpp',
            }
        end

        if _OPTIONS['enable-validation-layers'] then
            defines {
                'TEMPEST_ENABLE_VALIDATION_LAYERS',
            }
        end

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
