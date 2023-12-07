@echo off

dxc -spirv -T ps_6_5 -E PSMain water.hlsl -Fo water.px.spv
dxc -spirv -T vs_6_5 -E VSMain water.hlsl -Fo water.vx.spv