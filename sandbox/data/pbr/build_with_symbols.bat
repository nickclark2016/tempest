@echo off

dxc -spirv -T ps_6_5 -Zi -fspv-extension=SPV_KHR_non_semantic_info -fspv-extension=SPV_EXT_descriptor_indexing -fspv-debug=vulkan-with-source -E PSMain pbr.hlsl -Fo pbr.px.spv
dxc -spirv -T vs_6_5 -Zi -fspv-extension=SPV_KHR_non_semantic_info -fspv-extension=SPV_EXT_descriptor_indexing -fspv-debug=vulkan-with-source -E VSMain pbr.hlsl -Fo pbr.vx.spv
dxc -spirv -T ps_6_5 -Zi -fspv-extension=SPV_KHR_non_semantic_info -fspv-extension=SPV_EXT_descriptor_indexing -fspv-debug=vulkan-with-source -E ZPSMain pbr.hlsl -Fo pbr.z.px.spv
dxc -spirv -T vs_6_5 -Zi -fspv-extension=SPV_KHR_non_semantic_info -fspv-extension=SPV_EXT_descriptor_indexing -fspv-debug=vulkan-with-source -E ZVSMain pbr.hlsl -Fo pbr.z.vx.spv