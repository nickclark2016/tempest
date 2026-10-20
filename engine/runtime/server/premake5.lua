scoped.project('server', function()
    scoped.filter({
        'options:shared-engine',
    }, function()
        kind 'SharedLib'
    end)

    scoped.filter({
        'not options:shared-engine',
    }, function()
        kind 'StaticLib'
    end)
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**.hpp',
        'src/server_context.cpp',
    }

    includedirs {
        'include',
    }

    defines {
        'TEMPEST_HEADLESS_SERVER=1',
    }

    scoped.filter({
        'toolset:msc*',
    }, function()
        buildoptions {
            '/wd4324',
        }
    end)

    externalwarnings 'Off'
    warnings 'Extra'

    scoped.filter({ 'options:shared-engine' }, function()
        scoped.filter({ 'system:windows' }, function()
            defines {
                'TEMPEST_SERVER_API=__declspec(dllexport)',
            }
        end)
        scoped.filter({ 'system:linux' }, function()
            defines {
                'TEMPEST_SERVER_API=__attribute__((visibility("default")))',
            }
        end)
        scoped.filter({
            'toolset:clang',
            'system:windows',
        }, function()
            linkoptions {
                '-Wl,/implib:%{binaries}/server.lib',
            }
        end)
    end)

    scoped.usage("PUBLIC", function()
        uses {
            'tempest',
        }
    end)

    scoped.usage("server:includedirs", function()
        externalincludedirs {
            'include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'server:includedirs',
        }

        dependson {
            'server',
        }

        links {
            'server',
        }

        scoped.filter({ 'options:shared-engine', 'system:windows' }, function()
            defines {
                'TEMPEST_SERVER_API=__declspec(dllimport)',
            }
        end)
        scoped.filter({ 'not options:shared-engine or not system:windows' }, function()
            defines {
                'TEMPEST_SERVER_API=',
            }
        end)

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

scoped.project('tempest-server', function()
    kind 'ConsoleApp'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'src/main.cpp',
    }

    scoped.filter({
        'toolset:msc*',
    }, function()
        buildoptions {
            '/wd4324',
        }
    end)

    externalwarnings 'Off'
    warnings 'Extra'

    uses {
        'server',
    }
end)

scoped.group('Tests', function()
    scoped.project('server-tests', function()
        tags { 'non-gpu-test' }
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'tests/**.cpp',
        }

        scoped.filter({
            'toolset:msc*',
        }, function()
            buildoptions {
                '/wd4324',
            }
        end)

        externalwarnings 'Off'
        warnings 'Extra'

        uses {
            'server',
            'googletest',
        }
    end)
end)
