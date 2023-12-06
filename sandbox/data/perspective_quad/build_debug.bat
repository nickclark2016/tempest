@echo off

dxc -spirv -T ps_6_5 -Zi -fspv-extension=SPV_KHR_non_semantic_info -fspv-debug=vulkan-with-source -E PSMain perspective_quad.hlsl -Fo perspective_quad.px.spv
dxc -spirv -T vs_6_5 -Zi -fspv-extension=SPV_KHR_non_semantic_info -fspv-debug=vulkan-with-source -E VSMain perspective_quad.hlsl -Fo perspective_quad.vx.spv