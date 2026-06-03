scoped.project('game-editor', function()
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
        'editor-core',
        'game-runtime',
    }
end)