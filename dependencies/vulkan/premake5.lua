project 'vulkan'
    kind 'Utility'
    language 'C'
    cdialect 'C11'

    targetdir '%{binaries}'
    objdir '%{intermidates}'

    files {
        'include/**.h',
    }

    IncludeDir['vulkan'] = '%{root}/dependencies/vulkan/include'