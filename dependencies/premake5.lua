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

    if _OPTIONS['enable-aftermath'] then
        include 'aftermath-premake5.lua'
    end
end)
