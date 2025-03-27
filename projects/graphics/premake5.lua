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

        includedirs {
            'include',
        }

        prebuildcommands {
            '{MKDIR} "%{cfg.targetdir}/shaders"',
        }

        uses { 'imgui', 'vk-bootstrap', 'vma', 'vulkan' }

        scoped.usage("PUBLIC", function()
            uses {
                'core',
                'ecs',
                'logger',
                'math',
            }
        end)

        scoped.usage("INTERFACE", function()
            externalincludedirs {
                '%{root}/projects/graphics/include',
            }

            dependson {
                'graphics',
            }

            links {
                'graphics',
            }
        end)

        scoped.filter({ 'files:shaders/raster/**.slang' }, function()
            buildmessage 'Compiling %{file.relpath}'
            
            scoped.filter({
                'options:debugshaders'
            }, function()
                buildcommands {
                    '"%{fetch.slang.compiler}" "%{!file.relpath}" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.vert.spv -entry VSMain -O0 -g3 -line-directive-mode source-map',
                    '"%{fetch.slang.compiler}" "%{!file.relpath}" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.frag.spv -entry FSMain -O0 -g3 -line-directive-mode source-map',
                }
            end)

            scoped.filter({
                'options:not debugshaders'
            }, function()
                buildcommands {
                    '"%{fetch.slang.compiler}" "%{!file.relpath}" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.vert.spv -entry VSMain -O3',
                    '"%{fetch.slang.compiler}" "%{!file.relpath}" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.frag.spv -entry FSMain -O3',
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
                    '"%{fetch.slang.compiler}" "%{!file.relpath}" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.comp.spv -entry CSMain -O0 -g3 -line-directive-mode source-map',
                }
            end)

            scoped.filter({
                'options:not debugshaders'
            }, function()
                buildcommands {
                    '"%{fetch.slang.compiler}" "%{!file.relpath}" -target spirv -o %{cfg.targetdir}/shaders/%{file.basename}.comp.spv -entry CSMain -O3',
                }
            end)
            
            buildoutputs {
                '%{cfg.targetdir}/shaders/%{file.basename}.comp.spv',
            }

            buildinputs {
                'shaders/common/**.slang',
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
end)

newoption {
    trigger = 'debugshaders',
    description = 'Compile shaders with debug information',
    category = 'Tempest Engine',
}