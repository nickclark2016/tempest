scoped.group('Engine', function()
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
            -- Shaders
            'shaders/**.slang',
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

        externalincludedirs {
            '%{IncludeDir.glfw}',
            '%{IncludeDir.vkb}',
            '%{IncludeDir.vma}',
            '%{IncludeDir.vulkan}',
        }

        includedirs {
            'include',
            '%{IncludeDir.core}',
            '%{IncludeDir.ecs}',
            '%{IncludeDir.imgui}',
            '%{IncludeDir.logger}',
            '%{IncludeDir.math}',
        }

        IncludeDir['graphics'] = '%{root}/projects/graphics/include'

        prebuildcommands {
            '{MKDIR} "%{cfg.targetdir}/shaders"',
        }

        scoped.filter({ 'files:shaders/raster/**.slang' }, function()
            buildmessage 'Compiling %{file.relpath}'
            
            scoped.filter({
                'options:debugshaders'
            }, function()
                buildcommands {
                    '"%{fetch.slang}" "%{!file.relpath}" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.vert.spv -entry VSMain -O0 -g3 -line-directive-mode source-map',
                    '"%{fetch.slang}" "%{!file.relpath}" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.frag.spv -entry FSMain -O0 -g3 -line-directive-mode source-map',
                }
            end)

            scoped.filter({
                'options:not debugshaders'
            }, function()
                buildcommands {
                    '"%{fetch.slang}" "%{!file.relpath}" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.vert.spv -entry VSMain -O3',
                    '"%{fetch.slang}" "%{!file.relpath}" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.frag.spv -entry FSMain -O3',
                }
            end)

            
            buildoutputs {
                '%{cfg.targetdir}/shaders/%{file.basename}.vert.spv',
                '%{cfg.targetdir}/shaders/%{file.basename}.frag.spv'
            }

            buildinputs {
                'shaders/common/**.slang',
            }
        end)

        scoped.filter({ 'files:shaders/compute/**.slang' }, function()
            buildmessage 'Compiling %{file.relpath}'

            scoped.filter({
                'options:debugshaders'
            }, function()
                buildcommands {
                    '"%{fetch.slang}" "%{!file.relpath}" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.comp.spv -entry CSMain -O0 -g3 -line-directive-mode source-map',
                }
            end)

            scoped.filter({
                'options:not debugshaders'
            }, function()
                buildcommands {
                    '"%{fetch.slang}" "%{!file.relpath}" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.comp.spv -entry CSMain -O3',
                }
            end)
            
            buildoutputs {
                '%{cfg.targetdir}/shaders/%{file.basename}.comp.spv',
            }

            buildinputs {
                'shaders/common/**.slang',
            }
        end)

        externalwarnings 'Off'
        warnings 'Extra'
    end)
end)

newoption {
    trigger = 'debugshaders',
    description = 'Compile shaders with debug information',
    category = 'Tempest Engine',
}