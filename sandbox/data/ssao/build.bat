@echo off

dxc -spirv -T ps_6_5 -fspv-extension=SPV_EXT_descriptor_indexing -E PSMain ssao.hlsl -Fo ssao.px.spv
dxc -spirv -T ps_6_5 -fspv-extension=SPV_EXT_descriptor_indexing -E BlurMain ssao.hlsl -Fo ssao.blur.px.spv
dxc -spirv -T vs_6_5 -fspv-extension=SPV_EXT_descriptor_indexing -E VSMain ssao.hlsl -Fo ssao.vx.spv
