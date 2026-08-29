scoped.project('editor-entrypoint', function()
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'
    debugdir '.'
    
    scoped.filter({
        'system:not windows'
    }, function()
        kind 'ConsoleApp'
    end)
    
    scoped.filter({
        'system:windows'
    }, function()
        kind 'WindowedApp'

        links {
            'shell32'
        }
    end)

    files {
        'src/**.cpp',
        'src/**.hpp',
    }

    uses {
        'tempest',
        'editor-core',
    }

    dependson {
        'game-editor',
        'game-runtime',
    }

    scoped.filter({
        'system:not windows'
    }, function()
        linkgroups 'On'
    end)

    if _OPTIONS['enable-aftermath'] then
        uses 'aftermath-runtime'
    end
end)