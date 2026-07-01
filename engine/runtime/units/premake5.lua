scoped.project('units', function()
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**.hpp',
        'src/**.cpp',
    }

    includedirs {
        'include',
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
        uses {
            'api',
            'core',
            'math',
        }
    end)

    scoped.usage("units:includedirs", function()
        externalincludedirs {
            'include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'units:includedirs',
        }

        dependson {
            'units',
        }

        links {
            'units',
        }
    end)
end)
