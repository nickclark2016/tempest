project 'vulkan'
    kind 'Utility'
    language 'C'
    cdialect 'C11'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**.h',
    }

    usage "INTERFACE"
        externalincludedirs {
            '%{root}/dependencies/vulkan/include',
        }