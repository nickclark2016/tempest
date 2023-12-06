@echo off

dxc -spirv -T ps_6_5 -E PSMain perspective_quad.hlsl -Fo perspective_quad.px.spv
dxc -spirv -T vs_6_5 -E VSMain perspective_quad.hlsl -Fo perspective_quad.vx.spv