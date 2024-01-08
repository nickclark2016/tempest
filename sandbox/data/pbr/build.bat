@echo off

dxc -spirv -T ps_6_5 -fspv-extension=SPV_EXT_descriptor_indexing -E PSMain pbr.hlsl -Fo pbr.px.spv
dxc -spirv -T vs_6_5 -fspv-extension=SPV_EXT_descriptor_indexing -E VSMain pbr.hlsl -Fo pbr.vx.spv
dxc -spirv -T ps_6_5 -fspv-extension=SPV_EXT_descriptor_indexing -E ZPSMain pbr.hlsl -Fo pbr.z.px.spv
dxc -spirv -T vs_6_5 -fspv-extension=SPV_EXT_descriptor_indexing -E ZVSMain pbr.hlsl -Fo pbr.z.vx.spv