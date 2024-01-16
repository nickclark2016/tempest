@echo off

:: debug flags: -g3 -O1 -line-directive-mode source-map

..\..\..\dependencies\slang\slangc.exe pbr.slang -target spirv -o pbr.vert.spv -entry VSMain -O3 -line-directive-mode none
..\..\..\dependencies\slang\slangc.exe pbr.slang -target spirv -o pbr.frag.spv -entry FSMain -O3 -line-directive-mode none
