scoped.group('Vendor', function()
    include 'glfw'
    include 'googletest'
    include 'imgui'
    include 'miniz'
    include 'simdjson'
    include 'spdlog'
    include 'stb'
    include 'tinyexr'
    include 'tlsf'
    include 'vk-bootstrap'
    include 'vma'
    include 'vulkan'

    scoped.filter({
        'kind:StaticLib or SharedLib',
    }, function()
       pic 'On' 
    end)

    if _OPTIONS['enable-aftermath'] then
        include 'aftermath-premake5.lua'
    end
end)
