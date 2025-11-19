require('premake', '>=5.0.0-beta3')

fetch = require 'build/fetch'
require 'build/install'
scoped = require 'build/premake-scoped'

scoped.workspace('Tempest', function()
    configurations { 'Debug', 'Release', 'RelWithDebugInfo' }
    platforms { 'x64' }

    scoped.filter({
        'action:gmake*'
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
            'action:gmake*'
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
            'action:gmake*',
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
        flags {
            'MultiProcessorCompile',
        }
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
        'action:gmake*',
        'configurations:debug',
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
        'action:gmake*',
        'configurations:relwithdebuginfo',
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
        'toolset:clang',
        'system:windows',
        'action:gmake*',
        'configurations:release',
        'kind:ConsoleApp or SharedLib or WindowedApp'
    }, function()
        linkoptions {
            '-dll',
            '-Xlinker /NODEFAULTLIB:libcmt'
        }

        links {
            "libcmt"
        }
    end)

    scoped.filter({
        'configurations:Release'
    }, function()
        omitframepointer 'On'
    end)

    scoped.filter({
        'toolset:clang',
        'action:gmake*'
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

    startproject 'editor-entrypoint'

    include 'dependencies'
    include 'projects'
    include 'sandbox'
end)

newoption {
    trigger = 'use-asan',
    description = 'Use AddressSanitizer',
    category = 'Tempest Engine',
}