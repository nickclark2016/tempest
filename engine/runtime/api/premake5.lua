scoped.project('api', function()
    kind 'None'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**.hpp',
    }

    includedirs {
        'include',
    }

    uses {
        'core',
        'ecs',
        'logger',
        'math',
        'serialization',
    }

    scoped.usage('api:includedirs', function()
        externalincludedirs {
            '%{root}/engine/runtime/api/include',
        }
    end)

    scoped.usage("INTERFACE", function()
        dependson {
            'api',
        }

        scoped.filter({
            'options:shared-engine'
        }, function()
            defines {
                'TEMPEST_SHARED_LIB'
            }
        end)

        uses {
            'api:includedirs',
        }
    end)
end)