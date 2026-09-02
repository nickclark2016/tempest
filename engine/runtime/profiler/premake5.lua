scoped.project('profiler', function()
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**.hpp',
        'src/**.cpp',
        'src/**.hpp',
        'web/**.html',
        'web/**.css',
        'web/**.js',
    }

    includedirs {
        'include',
    }

    prebuildcommands {
        'premake5 --file="%{wks.basedir}/premake5.lua" embed-web-assets',
    }

    scoped.filter({ 'files:web/index.html' }, function()
        buildmessage 'Embedding web assets from %{file.relpath}'
        buildcommands {
            'premake5 --file=%{!wks.basedir}/premake5.lua embed-web-assets',
        }
        buildoutputs {
            '%{!wks.basedir}/engine/runtime/profiler/src/web_assets.cpp',
        }
        buildinputs {
            '%{!wks.basedir}/engine/runtime/profiler/web/styles.css',
            '%{!wks.basedir}/engine/runtime/profiler/web/app.js',
        }
    end)

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

    scoped.filter({
        'system:windows'
    }, function()
        links {
            'ws2_32',
        }
    end)

    scoped.usage("PUBLIC", function()
        uses { 'api', 'core', 'miniz' }
    end)

    scoped.usage("profiler:includedirs", function()
        externalincludedirs {
            'include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'profiler:includedirs',
        }

        dependson {
            'profiler',
        }

        links {
            'profiler',
        }

        scoped.filter({
            'system:windows'
        }, function()
            links {
                'ws2_32',
            }
        end)
    end)
end)

scoped.group('Tests', function()
    scoped.project('profiler-tests', function()
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'tests/**.cpp',
        }

        includedirs {
            'include',
        }

        uses {
            'tempest',
            'googletest',
            'simdjson',
        }

        scoped.filter({ 'system:windows' }, function()
            links { 'ws2_32' }
        end)

        scoped.filter({ 'system:linux' }, function()
            links { 'X11' }
            linkgroups 'On'
        end)

        warnings 'Extra'
    end)
end)
