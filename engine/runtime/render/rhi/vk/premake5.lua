scoped.project('rhi-vk', function()
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

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

    uses {
        'vk-bootstrap',
        'vulkan',
    }

    scoped.usage("rhi-vk:includedirs", function()
        externalincludedirs {
            'include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'rhi-vk:includedirs'
        }

        dependson {
            'rhi-vk',
        }

        links {
            'rhi-vk',
        }
    end)

    scoped.usage("PUBLIC", function()
        uses {
            'rhi-api',
            'vk-bootstrap',
            'vma',
        }
    end)
end)

scoped.group('Tests', function()
    scoped.project('rhi-vk-tests', function()
        kind 'ConsoleApp'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermediates}'

        files {
            'tests/**.cpp',
            'tests/**.hpp',
        }

        uses {
            'googletest',
            'rhi-vk',
            'tempest',
            'glfw',
        }
    end)
end)
