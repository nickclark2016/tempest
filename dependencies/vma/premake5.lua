project 'vma'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'
    
    targetdir '%{binaries}'
    objdir '%{intermidates}'

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
        '%{IncludeDir.vulkan}',
    }

    defines {
        'VMA_STATIC_VULKAN_FUNCTIONS=0',
        'VMA_DYNAMIC_VULKAN_FUNCTIONS=1',
    }

    IncludeDir['vma'] = '%{root}/dependencies/vma/include'