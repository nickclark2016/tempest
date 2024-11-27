project 'imgui'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermidates}'

    files {
        'include/**',
        'src/**',
    }
    
    includedirs {
        'include',
        '%{IncludeDir.glfw}',
        '%{IncludeDir.vulkan}',
        '%{IncludeDir.vma}',
    }

    dependson {
        'glfw',
        'vma',
    }

    links {
        'glfw',
        'vma',
    }

    defines {
        'IMGUI_IMPL_VULKAN_NO_PROTOTYPES',
    }

    warnings 'Off'

    IncludeDir['imgui'] = '%{root}/dependencies/imgui/include'