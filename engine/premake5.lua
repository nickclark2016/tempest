scoped.group('Engine', function()
    scoped.filter({
        'system:windows',
    }, function()
        defines {
            'TEMPEST_PLATFORM_WINDOWS',
        }
    end)

    scoped.filter({
        'system:linux',
    }, function()
        defines {
            'TEMPEST_PLATFORM_LINUX',
        }
    end)

    scoped.filter({
        'kind:StaticLib or SharedLib',
    }, function()
       pic 'On' 
    end)

    include 'editor/premake5.lua'
    include 'runtime/premake5.lua'
end)

newoption {
    trigger = 'shared-engine',
    description = 'Build the engine as a shared library',
    category = 'Tempest Engine'
}