dxc -spirv -T ps_6_5 -E PSMain blit.hlsl -Fo blit.px.spv
dxc -spirv -T vs_6_5 -E VSMain blit.hlsl -Fo blit.vx.spv