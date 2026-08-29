scoped.project('graphics', function()
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

    uses { 'imgui', 'vk-bootstrap', 'vma', 'vulkan', 'rhi-vk' }

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
            'ecs',
            'logger',
            'math',
            'rhi-api',
        }

        links {
            'rhi-vk',
        }
    end)

    scoped.usage("graphics:includedirs", function()
        externalincludedirs {
            'include',
        }
    end)

    scoped.usage("INTERFACE", function()
        uses {
            'graphics:includedirs',
        }

        dependson {
            'graphics',
        }

        links {
            'graphics',
            'vk-bootstrap',
            'vma',
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
end)

scoped.group('Tests', function()
    scoped.project('graphics-tests', function()
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

        linkgroups 'On'
    
        externalwarnings 'Off'
        warnings 'Extra'
    end)
end)

newoption {
    trigger = 'debug-shaders',
    description = 'Compile shaders with debug information',
    category = 'Tempest Engine',
}

newoption {
    trigger = 'enable-validation-layers',
    description = 'Enable Vulkan validation layers',
    category = 'Tempest Engine',
}