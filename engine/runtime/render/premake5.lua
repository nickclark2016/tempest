scoped.group('render', function()
    include 'rhi'
    include 'graph'
    include 'system'
end)

newoption {
    trigger = 'debug-shaders',
    description = 'Compile shaders with debug information',
    category = 'Tempest Engine',
}