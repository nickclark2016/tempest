scoped.project('core', function()
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**.hpp',
        'src/**.cpp',
        'src/**.hpp',
        'natvis/**.natvis',
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

    scoped.filter({
        'system:linux'
    }, function()
        links {
            'dl',
            'pthread',
            'atomic',
        }
    end)

    externalwarnings 'Off'
    warnings 'Extra'

    uses { 'glfw', 'tlsf', 'yyjson' }

    scoped.filter({
        'options:shared-engine',
    }, function()
        defines {
            'TEMPEST_API_EXPORT'
        }
    end)

    scoped.usage("core:defines", function()
        scoped.filter({
            'system:windows'
        }, function()
            defines {
                'TEMPEST_WIN_THREADS',
            }
        end)

        scoped.filter({
            'system:linux'
        }, function()
            defines {
                'TEMPEST_POSIX_THREADS',
            }
        end)
    end)

    scoped.usage("PUBLIC", function()
        uses { 'api', 'core:defines' }
    end)

    scoped.usage("core:includedirs", function()
        externalincludedirs {
            'include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'core:includedirs',
        }

        dependson {
            'core',
        }

        links {
            'core',
            'glfw',
            'tlsf',
            'yyjson',
        }

        scoped.filter({
            'system:linux',
            'kind:not StaticLib',
        }, function()
            links {
                'atomic',
            }
        end)

        scoped.filter({
            'system:windows',
            'kind:not StaticLib',
        }, function()
            links {
                'Synchronization',
            }
        end)

        scoped.filter({
            'system:windows',
            'toolset:not msc*'
        }, function()
            links {
                'bcrypt',
            }
        end)
    end)
end)

scoped.group('Tests', function()
    scoped.project('core-tests', function()
        tags { 'non-gpu-test' }
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'
    
        targetdir '%{binaries}'
        objdir '%{intermediates}'
    
        files {
            'tests/**.cpp',
        }

        uses {
            'tempest',
            'googletest',
        }

        scoped.filter({ 'system:linux' }, function()
            links { 'atomic' }
        end)
    
        externalwarnings 'Off'
        warnings 'Extra'
    end)
end)
