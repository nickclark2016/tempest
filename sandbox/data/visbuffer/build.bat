@echo off

dxc -spirv -T ps_6_5 -E PSMain visbuffer.hlsl -Fo visbuffer.px.spv
dxc -spirv -T vs_6_5 -E VSMain visbuffer.hlsl -Fo visbuffer.vx.spv
dxc -spirv -T cs_6_5 -E CSMain material_count.hlsl -Fo material_count.cx.spv
dxc -spirv -T cs_6_5 -E CSMain material_start.hlsl -Fo material_start.cx.spv
dxc -spirv -T cs_6_5 -E CSMain material_pixel_sort.hlsl -Fo material_pixel_sort.cx.spv