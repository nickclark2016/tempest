scoped.project('game', function()
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
end)