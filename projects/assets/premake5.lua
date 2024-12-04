scoped.group('Engine', function()
    scoped.project('assets', function()
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
            '%{IncludeDir.core}',
            '%{IncludeDir.ecs}',
            '%{IncludeDir.glfw}',
            '%{IncludeDir.logger}',
            '%{IncludeDir.math}',
            '%{IncludeDir.simdjson}',
            '%{IncludeDir.spdlog}',
            '%{IncludeDir.stb}',
            '%{IncludeDir.tlsf}',
        }

        links {
            'core',
            'ecs',
            'glfw',
            'logger',
            'math',
            'simdjson',
            'spdlog',
            'tlsf',
        }

        dependson {
            'core',
            'ecs',
            'glfw',
            'logger',
            'math',
            'simdjson',
            'spdlog',
            'tlsf',
        }

        externalwarnings 'Off'
        
        IncludeDir['assets'] = '%{root}/projects/assets/include'
    end)
end)
