group 'Engine'
    project 'graphics'
        kind 'StaticLib'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermidates}'

        files {
            'include/**.hpp',
            'src/**.cpp',
            'src/**.hpp',
        }

        dependson {
            'core',
            'ecs',
            'glfw',
            'imgui',
            'math',
            'logger',
            'simdjson',
            'spdlog',
            'stb',
            'tlsf',
            'vk-bootstrap',
            'vma',
        }

        links {
            'core',
            'ecs',
            'glfw',
            'imgui',
            'math',
            'logger',
            'simdjson',
            'spdlog',
            'stb',
            'tlsf',
            'vk-bootstrap',
            'vma',
        }

        includedirs {
            'include',
            '%{IncludeDir.core}',
            '%{IncludeDir.ecs}',
            '%{IncludeDir.glfw}',
            '%{IncludeDir.imgui}',
            '%{IncludeDir.logger}',
            '%{IncludeDir.math}',
            '%{IncludeDir.vkb}',
            '%{IncludeDir.vma}',
            '%{IncludeDir.vulkan}',
        }

        IncludeDir['graphics'] = '%{root}/projects/graphics/include'