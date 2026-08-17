scoped.project('rhi-examples', function()
    kind 'ConsoleApp'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'src/**.cpp',
        'src/**.hpp',
        'shaders/**.slang',
    }

    includedirs {
        'src',
        '%{cfg.objdir}/shaders',
    }

    scoped.filter({ 'files:shaders/triangle.slang' }, function()
        buildmessage 'Compiling %{file.relpath}'
        buildcommands {
            '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -entry VSMain -stage vertex -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name triangle_vs_spv -o %{!cfg.objdir}/shaders/triangle.vert.h',
            '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -entry FSMain -stage fragment -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name triangle_fs_spv -o %{!cfg.objdir}/shaders/triangle.frag.h',
        }
        buildoutputs {
            '%{!cfg.objdir}/shaders/triangle.vert.h',
            '%{!cfg.objdir}/shaders/triangle.frag.h',
        }
    end)

    uses {
        'rhi-vk',
        'rhi-api',
        'tempest',
        'glfw',
    }
end)
