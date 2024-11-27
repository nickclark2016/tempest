project 'vk-bootstrap'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermidates}'

    files {
        'include/**.h',
        'src/**.cpp',
    }

    includedirs {
        'include',
        '%{IncludeDir.vulkan}',
    }

    IncludeDir['vkb'] = '%{root}/dependencies/vk-bootstrap/include'