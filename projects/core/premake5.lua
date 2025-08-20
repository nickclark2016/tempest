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
            'pthread',
        }
    end)

    externalwarnings 'Off'
    warnings 'Extra'

    uses { 'glfw', 'tlsf' }

    scoped.usage("PUBLIC", function()
        uses { 'math' }

        scoped.filter({
            'system:windows'
        }, function()
            defines {
                'TEMPEST_WIN_THREADS',
            }

            links {
                'Advapi32.lib'
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

    scoped.usage("INTERFACE", function()
        externalincludedirs {
            '%{root}/projects/core/include',
        }

        dependson {
            'core',
        }

        links {
            'core',
        }

        scoped.filter({
            'system:windows'
        }, function()
            links {
                'Advapi32.lib'
            }
        end)
    end)
end)

scoped.group('Tests', function()
    scoped.project('core-tests', function()
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'
    
        targetdir '%{binaries}'
        objdir '%{intermediates}'
    
        files {
            'tests/**.cpp',
        }

        uses {
            'core',
            'glfw',
            'googletest',
            'tlsf',
        }
    
        externalwarnings 'Off'
        warnings 'Extra'
    end)
end)
