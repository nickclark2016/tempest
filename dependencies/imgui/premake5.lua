project 'imgui'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**',
        'src/**',
    }
    
    includedirs {
        'include',
    }

    defines {
        'IMGUI_IMPL_VULKAN_NO_PROTOTYPES',
    }

    warnings 'Off'

    usage "PUBLIC"
        uses { 'glfw', 'vma', 'vulkan' }

    usage "INTERFACE"
        externalincludedirs {
            '%{root}/dependencies/imgui/include',
        }

        dependson {
            'imgui',
        }

        links {
            'imgui',
        }