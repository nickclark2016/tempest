project 'vk-bootstrap'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'include/**.h',
        'src/**.cpp',
    }

    includedirs {
        'include',
    }

    usage "PUBLIC"
        uses { "vulkan" }

    usage "INTERFACE"
        externalincludedirs {
            '%{root}/dependencies/vk-bootstrap/include',
        }

        dependson {
            'vk-bootstrap',
        }

        links {
            'vk-bootstrap',
        }