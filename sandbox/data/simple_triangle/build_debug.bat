@echo off

dxc -spirv -T ps_6_5 -Zi -fspv-extension=SPV_KHR_non_semantic_info -fspv-debug=vulkan-with-source -E PSMain simple_triangle.hlsl -Fo simple_triangle.px.spv
dxc -spirv -T vs_6_5 -Zi -fspv-extension=SPV_KHR_non_semantic_info -fspv-debug=vulkan-with-source -E VSMain simple_triangle.hlsl -Fo simple_triangle.vx.spv