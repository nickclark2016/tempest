require('premake', '>=5.0.0-beta3')

fetch = require 'build/fetch'
require 'build/install'
scoped = require 'build/premake-scoped'
require 'build/tests'

scoped.workspace('Tempest', function()
    configurations { 'Debug', 'Release', 'RelWithDebugInfo' }
    platforms { 'x64' }
    defaultplatform 'x64'
    location 'build/%{_ACTION}'

    if _ACTION == 'compilecommands' then
        os.mkdir('build/compilecommands')
    end

    scoped.filter({
        'action:not vs*'
    }, function()
         toolset 'clang'
    end)

    scoped.filter({
        'action:vs*'
    }, function()
        toolset 'v143'
        usestandardpreprocessor 'On'
    end)

    scoped.filter({
        'action:vs2026'
    }, function()
        toolset 'v145'
    end)

    scoped.filter({
        'configurations:Debug'
    }, function()
        defines { '_DEBUG' }

        scoped.filter({
            'action:vs*'
        }, function()
            symbols 'Full'
        end)

        scoped.filter({
            'action:not vs*'
        }, function()
            symbols 'On'
        end)
    end)

    scoped.filter({
        'configurations:Release'
    }, function()
        defines { 'NDEBUG' }
        optimize 'Full'
    end)

    scoped.filter({
        'configurations:RelWithDebugInfo'
    }, function()
        defines { 'NDEBUG' }
        optimize 'On'
    end)

    scoped.filter({
        'configurations:Debug or RelWithDebugInfo',
    }, function()
        scoped.filter({
            'action:vs*',
        }, function()
            symbols 'Full'
        end)
        
        scoped.filter({
            'action:not vs*',
        }, function()
            symbols 'On'
        end)
    end)

    scoped.filter({
        'platforms:x64'
    }, function()
        architecture 'x86_64'
    end)

    scoped.filter({
        'system:windows'
    }, function()
        systemversion 'latest'
        staticruntime 'Off'
    end)

    root = path.getdirectory(_MAIN_SCRIPT)

    binaries = '%{root}/bin/%{cfg.buildcfg}/%{cfg.system}-%{cfg.toolset}'
    intermediates = '%{root}/bin-int/%{cfg.buildcfg}/%{cfg.system}-%{cfg.toolset}'

    scoped.filter({
        'action:vs*'
    }, function()
        multiprocessorcompile 'On'
    end)

    scoped.filter({
        'action:export-compile-commands',
        'toolset:clang',
    }, function()
        buildoptions {
            '-march=native'
        }
    end)

    scoped.filter({
        'toolset:clang',
        'system:windows',
        'action:not vs*',
        'configurations:Debug',
        'kind:ConsoleApp or SharedLib or WindowedApp'
    }, function()
        linkoptions {
            '-dll_dbg',
            '-Xlinker /NODEFAULTLIB:libcmt'
        }

        links {
            "libcmtd"
        }
    end)

    scoped.filter({
        'toolset:clang',
        'system:windows',
        'action:not vs*',
        'configurations:RelWithDebugInfo or Release',
        'kind:ConsoleApp or SharedLib or WindowedApp'
    }, function()
        linkoptions {
            '-dll',
            '-Xlinker /NODEFAULTLIB:libcmt'
        }

        links {
            "libcmtd"
        }
    end)

    scoped.filter({
        'configurations:Release'
    }, function()
        omitframepointer 'On'
    end)

    scoped.filter({
        'toolset:clang',
        'action:not vs*'
    }, function()
        disablewarnings {
            'missing-designated-field-initializers',
        }
    end)

    scoped.filter({
        'options:use-asan'
    }, function()
        sanitize { 'Address' }
    end)

    scoped.filter({
        'toolset:msc*',
        'configurations:RelWithDebugInfo',
    }, function()
        dynamicdebugging 'On'
    end)

    scoped.filter({
        'toolset:clang*',
        'system:windows',
        'action:ninja',
        'configurations:Debug',
    }, function()
        buildoptions {
            '-gcodeview'
        }

        linkoptions {
            '-Wl,/DEBUG'
        }
    end)

    startproject 'editor-entrypoint'

    include 'dependencies'
    include 'engine'
    include 'sandbox'
end)

newoption {
    trigger = 'use-asan',
    description = 'Use AddressSanitizer',
    category = 'Tempest Engine',
}
