@echo off

dxc -spirv -T ps_6_5 -Zi -Od -fspv-extension=SPV_KHR_non_semantic_info -fspv-extension=SPV_EXT_descriptor_indexing -fspv-debug=vulkan-with-source -E PSMain ssao.hlsl -Fo ssao.px.spv
dxc -spirv -T ps_6_5 -Zi -Od -fspv-extension=SPV_KHR_non_semantic_info -fspv-extension=SPV_EXT_descriptor_indexing -fspv-debug=vulkan-with-source -E BlurMain ssao.hlsl -Fo ssao.blur.px.spv
dxc -spirv -T vs_6_5 -Zi -Od -fspv-extension=SPV_KHR_non_semantic_info -fspv-extension=SPV_EXT_descriptor_indexing -fspv-debug=vulkan-with-source -E VSMain ssao.hlsl -Fo ssao.vx.spv