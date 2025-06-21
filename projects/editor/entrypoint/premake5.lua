scoped.project('editor-entrypoint', function()
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'
    
    scoped.filter({
        'system:not windows'
    }, function()
        kind 'ConsoleApp'
    end)
    
    scoped.filter({
        'system:windows'
    }, function()
        kind 'WindowedApp'
    end)

    files {
        'src/**.cpp',
        'src/**.hpp',
    }

    uses {
        'tempest'
    }
end)