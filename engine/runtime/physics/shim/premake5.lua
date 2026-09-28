scoped.project('jolt-shim', function()
    kind 'SharedLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'src/**.cpp',
        'include/**.hpp',
    }

    includedirs {
        'include',
    }

    defines {
        'JOLT_SHIM_EXPORTS',
        'JPH_CROSS_PLATFORM_DETERMINISTIC',
    }

    scoped.filter({
        'configurations:Debug',
    }, function()
        optimize 'Off'
        defines {
            'JPH_ENABLE_ASSERTS',
        }
    end)

    scoped.filter({
        'toolset:clang',
        'system:windows',
        'action:not vs*',
        'configurations:Debug',
    }, function()
        linkoptions {
            '-dll_dbg',
            '-Xlinker /NODEFAULTLIB:libcmt',
        }

        links {
            'msvcrtd',
        }
    end)

    scoped.filter({
        'toolset:clang',
        'system:windows',
        'action:not vs*',
        'configurations:RelWithDebugInfo or Release',
    }, function()
        linkoptions {
            '-dll',
            '-Xlinker /NODEFAULTLIB:libcmt',
        }

        links {
            'msvcrt',
        }
    end)

    uses {
        'jolt',
    }

    scoped.usage("jolt-shim:includedirs", function()
        externalincludedirs {
            'include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'jolt-shim:includedirs',
        }

        dependson {
            'jolt-shim',
        }

        links {
            'jolt-shim',
        }
    end)
end)

scoped.group('Tests', function()
    scoped.project('jolt-shim-tests', function()
        tags { 'non-gpu-test' }
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
            'jolt-shim',
            'tempest',
            'core',
            'job',
            'logger',
            'googletest',
        }

        scoped.filter({ 'system:linux' }, function()
            links { 'pthread', 'dl', 'atomic' }
        end)
    end)
end)
