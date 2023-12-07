@echo off

dxc -spirv -T ps_6_5 -Zi -fspv-extension=SPV_KHR_non_semantic_info -fspv-debug=vulkan-with-source -E PSMain water.hlsl -Fo water.px.spv
dxc -spirv -T vs_6_5 -Zi -fspv-extension=SPV_KHR_non_semantic_info -fspv-debug=vulkan-with-source -E VSMain water.hlsl -Fo water.vx.spv