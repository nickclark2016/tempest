@echo off

dxc -spirv -T ps_6_5 -E PSMain simple_quad.hlsl -Fo simple_quad.px.spv
dxc -spirv -T vs_6_5 -E VSMain simple_quad.hlsl -Fo simple_quad.vx.spv