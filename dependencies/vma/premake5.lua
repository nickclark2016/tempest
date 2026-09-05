project 'vma'
    kind 'Utility'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/vk_mem_alloc.h',
    }

    usage "PUBLIC"
        uses { 'vulkan' }

    usage "INTERFACE"
        externalincludedirs {
            'include',
        }