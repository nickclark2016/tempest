project 'vma'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'
    
    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/vk_mem_alloc.h',
        'src/Common.cpp',
        'src/Common.h',
        'src/VmaUsage.cpp',
        'src/VmaUsage.h',
    }

    warnings 'Off'

    includedirs {
        'include',
    }

    defines {
        'VMA_STATIC_VULKAN_FUNCTIONS=0',
        'VMA_DYNAMIC_VULKAN_FUNCTIONS=1',
    }

    usage "PUBLIC"
        uses { 'vulkan' }

    usage "INTERFACE"
        externalincludedirs {
            '%{root}/dependencies/vma/include',
        }

        dependson {
            'vma',
        }

        links {
            'vma',
        }