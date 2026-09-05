scoped.group('Vendor', function()
    include 'glfw'
    include 'googletest'
    include 'imgui'
    include 'miniz'
    include 'yyjson'
    include 'stb'
    include 'tinyexr'
    include 'tlsf'
    include 'vma'
    include 'vulkan'

    scoped.filter({
        'kind:StaticLib or SharedLib',
        'system:not windows',
    }, function()
       pic 'On' 
    end)

    if _OPTIONS['enable-aftermath'] then
        include 'aftermath-premake5.lua'
    end
end)
