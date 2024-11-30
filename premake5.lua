require('premake', '>=5.0.0-beta3')
fetch = require 'build/fetch'
scoped = require 'build/premake-scoped'

scoped.workspace('Tempest', function()
    configurations { 'Debug', 'Release' }
    platforms { 'x64' }

    filter 'configurations:Debug'
        defines { '_DEBUG' }
        symbols 'On'

    filter 'configurations:Release'
        defines { 'NDEBUG' }
        optimize 'On'

    filter 'platforms:x64'
        architecture 'x86_64'

    filter 'system:windows'
        systemversion 'latest'
        staticruntime 'Off'

    filter {}

    root = path.getdirectory(_MAIN_SCRIPT)

    binaries = '%{root}/bin/%{cfg.buildcfg}/%{cfg.system}/%{cfg.architecture}'
    intermidates = '%{root}/bin-int/%{cfg.buildcfg}/%{cfg.system}/%{cfg.architecture}'

    filter { 'action:vs*' }
        flags {
            'MultiProcessorCompile',
        }

    filter {}

    IncludeDir = {}

    startproject 'editor'

    include 'dependencies'
    include 'projects'
end)
