scoped.project('game-runtime', function()
    kind 'SharedLib'
    language 'C++'
    cppdialect 'C++20'
    
    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'src/**.cpp',
        'src/**.hpp',
    }

    uses {
        'tempest',
    }

    scoped.usage('PUBLIC', function()
        externalincludedirs {
            '%{root}/game/runtime/include',
        }
    end)

    scoped.usage('INTERFACE', function()
        links {
            'game-runtime',
        }
    end)
end)