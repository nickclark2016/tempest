scoped.project('editor-ui', function()
    language 'C++'
    cppdialect 'C++20'
    kind 'StaticLib'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'src/**.cpp',
        'src/**.hpp',
        'include/**.hpp',
    }

    includedirs {
        'include',
    }

    scoped.usage('INTERFACE', function()
        externalincludedirs {
            '%{root}/projects/editor/ui/include',
        }

        dependson {
            'editor-ui',
        }

        links {
            'editor-ui',
        }
    end)

    scoped.usage('PUBLIC', function()
        uses {
            'rhi-api',
        }
    end)

    uses {
        'imgui',
    }
end)