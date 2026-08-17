newoption {
    trigger = "rhi-vulkan",
    description = "Enable Vulkan backend for RHI",
    category = "RHI Backends"
}

local supported_apis = {}

if _OPTIONS["rhi-vulkan"] then
    table.insert(supported_apis, 'vk')
end

scoped.group('rhi', function()
    include 'api'

    if table.contains(supported_apis, 'vk') then
        defines { 'TEMPEST_RHI_VULKAN' }
        include 'vk'
        include 'examples'
    end    
end)