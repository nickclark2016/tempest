scoped.project('job', function()
    kind 'StaticLib'

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
            'core',
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
        }

        includedirs {
            'include',
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
