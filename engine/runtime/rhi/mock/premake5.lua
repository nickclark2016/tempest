scoped.group('RHI', function()
    scoped.project('rhi-mock', function()
        kind 'StaticLib'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'include/tempest/rhi/mock/*.hpp',
            'src/*.cpp',
            'src/*.hpp',
        }

        includedirs {
            'include',
        }

        uses {
            'rhi-api',
        }

        warnings 'Extra'

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
            }
        end)

        scoped.usage('INTERFACE', function()
            includedirs {
                'include',
            }

            uses {
                'rhi-api'
            }

            links {
                'rhi-mock',
            }

            dependson {
                'rhi-mock'
            }
        end)
    end)

    scoped.project('rhi-mock-tests', function()
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'tests/*.cpp',
        }

        uses {
            'googletest',
            'rhi-mock',
            'tempest',
        }

        warnings 'Extra'
    end)
end)
