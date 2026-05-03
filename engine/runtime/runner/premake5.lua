scoped.project('runner', function()
    scoped.filter({
        'system:windows'
    }, function()
        kind 'WindowedApp'

        links {
            'shell32'
        }
    end)

    scoped.filter({
        'system:not windows'
    }, function()
        kind 'ConsoleApp'
    end)

    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'

    files {
        'src/**.hpp',
        'src/**.cpp',
    }

    uses {
        'tempest'
    }

    dependson {
        'game'
    }
end)