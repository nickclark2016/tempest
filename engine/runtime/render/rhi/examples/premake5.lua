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

    scoped.filter({
        'options:shared-engine',
    }, function()
        defines {
            'TEMPEST_SHARED_LIB',
        }
    end)

    scoped.filter({ 'files:shaders/triangle.slang' }, function()
        buildmessage 'Compiling %{file.relpath}'
        buildcommands {
            '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -entry VSMain -stage vertex -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name triangle_vs_spv -o %{!cfg.objdir}/shaders/triangle.vert.h',
            '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -entry FSMain -stage fragment -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name triangle_fs_spv -o %{!cfg.objdir}/shaders/triangle.frag.h',
            'touch %{!cfg.objdir}/shaders/triangle.vert.h',
            'touch %{!cfg.objdir}/shaders/triangle.frag.h',
        }
        buildoutputs {
            '%{!cfg.objdir}/shaders/triangle.vert.h',
            '%{!cfg.objdir}/shaders/triangle.frag.h',
        }
    end)

    scoped.filter({ 'files:shaders/postprocess.slang' }, function()
        buildmessage 'Compiling %{file.relpath}'
        buildcommands {
            '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -entry PostProcessCSMain -stage compute -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name postprocess_cs_spv -o %{!cfg.objdir}/shaders/postprocess.comp.h',
            'touch %{!cfg.objdir}/shaders/postprocess.comp.h',
        }
        buildoutputs {
            '%{!cfg.objdir}/shaders/postprocess.comp.h',
        }
    end)

    scoped.filter({ 'files:shaders/animate.slang' }, function()
        buildmessage 'Compiling %{file.relpath}'
        buildcommands {
            '%{!fetch.slang.compiler} %{!file.abspath} -target spirv -capability SPIRV_1_5 -entry AnimateCSMain -stage compute -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name animate_cs_spv -o %{!cfg.objdir}/shaders/animate.comp.h',
            'touch %{!cfg.objdir}/shaders/animate.comp.h',
        }
        buildoutputs {
            '%{!cfg.objdir}/shaders/animate.comp.h',
        }
    end)

    uses {
        'tempest',
        'vk-bootstrap',
        'vulkan',
        'glfw',
    }
end)
