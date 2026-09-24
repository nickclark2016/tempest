project 'jolt'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**.h',
        'include/**.inl',
        'src/**.cpp',
        'src/**.h',
    }

    includedirs {
        'include',
    }

    warnings 'Off'

    defines {
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

    usage 'INTERFACE'
        externalincludedirs {
            'include',
        }

        dependson {
            'jolt',
        }

        links {
            'jolt',
        }
