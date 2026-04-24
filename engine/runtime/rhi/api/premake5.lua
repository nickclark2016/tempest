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

        scoped.filter({
            'options:shared-engine',
        }, function()
            defines {
                'TEMPEST_API_EXPORT'
            }
        end)

        scoped.usage('PUBLIC', function()
            uses {
                'api',
                'core'
            }
        end)

        scoped.usage("rhi-api:includedirs", function()
            externalincludedirs {
                '%{root}/engine/runtime/rhi/api/include',
            }
        end)

        scoped.usage('INTERFACE', function()
            uses {
                'rhi-api:includedirs',
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
