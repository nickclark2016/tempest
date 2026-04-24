scoped.group('Engine', function()
    include 'editor/premake5.lua'
    include 'runtime/premake5.lua'
end)

newoption {
    trigger = 'shared-engine',
    description = 'Build the engine as a shared library',
    category = 'Tempest Engine'
}