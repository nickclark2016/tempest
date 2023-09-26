@echo off

dxc -spirv -T ps_6_5 -E PSMain simple_triangle.hlsl -Fo simple_triangle.px.spv
dxc -spirv -T vs_6_5 -E VSMain simple_triangle.hlsl -Fo simple_triangle.vx.spv