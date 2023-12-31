@echo off

dxc -spirv -T cs_6_5 -E InitializeFFTState fft_water.hlsl -Fo fft_water.init_state.cx.spv
dxc -spirv -T cs_6_5 -E PackSpectrumConjugate fft_water.hlsl -Fo fft_water.pack_spectrum.cx.spv
dxc -spirv -T cs_6_5 -E UpdateSpectrumForFFT fft_water.hlsl -Fo fft_water.update_spectrum.cx.spv
dxc -spirv -T cs_6_5 -E HorizontalFFT fft_water.hlsl -Fo fft_water.horizontal_fft.cx.spv
dxc -spirv -T cs_6_5 -E VerticalFFT fft_water.hlsl -Fo fft_water.vertical_fft.cx.spv
dxc -spirv -T cs_6_5 -E AssembleMaps fft_water.hlsl -Fo fft_water.assemble_maps.cx.spv
dxc -spirv -T vs_6_5 -E VSMain fft_water_gfx.hlsl -Fo fft_water.vx.spv
dxc -spirv -T ps_6_5 -E PSMain fft_water_gfx.hlsl -Fo fft_water.px.spv