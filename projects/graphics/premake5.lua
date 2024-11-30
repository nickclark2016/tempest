scoped.group('Engine', function()
    scoped.project('graphics', function()
        kind 'StaticLib'
        language 'C++'
        cppdialect 'C++20'

        targetdir '%{binaries}'
        objdir '%{intermidates}'

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

        scoped.filter({ 'files:shaders/raster/**.slang' }, function()
            buildmessage 'Compiling %{file.relpath}'

            buildcommands {
                '"%{fetch.slang}" "%[%{!file.relpath}]" -I"%{prj.basedir}/shaders/common" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.vert.spv -entry VSMain -O3',
                '"%{fetch.slang}" "%[%{!file.relpath}]" -I"%{prj.basedir}/shaders/common" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.frag.spv -entry FSMain -O3',
            }
            
            buildoutputs {
                '%{cfg.targetdir}/shaders/%{file.basename}.vert.spv',
                '%{cfg.targetdir}/shaders/%{file.basename}.frag.spv'
            }
        end)

        scoped.filter({ 'files:shaders/compute/**.slang' }, function()
            buildmessage 'Compiling %{file.relpath}'

            buildcommands {
                '"%{fetch.slang}" "%[%{!file.relpath}]" -I"%{prj.basedir}/shaders/common" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.comp.spv -entry CSMain -O3',
            }
            
            buildoutputs {
                '%{cfg.targetdir}/shaders/%{file.basename}.comp.spv',
            }
        end)
    end)
end)
