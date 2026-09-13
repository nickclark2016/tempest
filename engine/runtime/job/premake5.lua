scoped.project('job', function()
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
    tags { 'non-gpu-test' }

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

    scoped.usage("PUBLIC", function()
        uses {
            'api',
            'core',
            'logger',
            'profiler',
        }
    end)

    scoped.filter({ 'system:linux' }, function()
        links { 'pthread', 'dl', 'atomic' }
    end)

    scoped.usage("job:includedirs", function()
        externalincludedirs {
            'include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'job:includedirs',
        }

        dependson {
            'job',
        }

        links {
            'job',
        }
    end)
end)

scoped.group('Tests', function()
    scoped.project('job-tests', function()
        tags { 'non-gpu-test' }
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'tests/**.cpp',
            'tests/**.hpp',
            'src/**.cpp',
        }

        includedirs {
            'include',
            'src',
        }

        uses {
            'googletest',
            'tempest',
        }

        scoped.filter({ 'system:linux' }, function()
            links { 'pthread', 'dl', 'atomic' }
        end)
    end)
end)
