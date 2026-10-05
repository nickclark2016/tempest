scoped.project('tempest-server', function()
    kind 'ConsoleApp'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**.hpp',
        'src/**.hpp',
        'src/**.cpp',
    }

    includedirs {
        'include',
    }

    defines {
        'TEMPEST_HEADLESS_SERVER=1',
    }

    externalwarnings 'Off'
    warnings 'Extra'

    scoped.filter({
        'toolset:msc*'
    }, function()
        buildoptions {
            '/wd4324',
        }
    end)

    uses {
        'physics',
        'tempest',
    }

    scoped.filter({ 'system:windows' }, function()
        links {
            'ws2_32',
        }
    end)

    scoped.filter({ 'system:linux' }, function()
        links {
            'pthread',
            'dl',
            'atomic',
        }
    end)
end)
