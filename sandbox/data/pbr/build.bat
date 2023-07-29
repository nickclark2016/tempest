dxc -spirv -T ps_6_5 -E PSMain pbr.hlsl -Fo pbr.px.spv
dxc -spirv -T vs_6_5 -E VSMain pbr.hlsl -Fo pbr.vx.spv