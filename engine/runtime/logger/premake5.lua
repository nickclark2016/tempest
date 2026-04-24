scoped.project('logger', function()
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

    defines {
        '_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS',
    }

    scoped.filter({
        'toolset:msc*'
    }, function()
        buildoptions {
            '/wd4324', -- 'structure was padded due to alignment specifier'
        }
    end)

    externalwarnings 'Off'
    warnings 'Extra'

    scoped.filter({
        'options:shared-engine',
    }, function()
        defines {
            'TEMPEST_API_EXPORT'
        }
    end)

    scoped.usage("PUBLIC", function()
        uses { 'api', 'core' }
    end)

    scoped.usage("logger:includedirs", function()
        externalincludedirs {
            '%{root}/engine/runtime/logger/include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'logger:includedirs',
        }

        dependson {
            'logger',
        }

        links {
            'logger',
        }
    end)
end)
