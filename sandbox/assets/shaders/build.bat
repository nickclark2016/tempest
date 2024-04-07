@echo off

:: debug flags: -g3 -O1 -line-directive-mode source-map

:: check for debug flag
if "%1" == "debug" (
    set debug_flags=-g3 -O1 -line-directive-mode source-map
) else (
    set debug_flags=-O3 -line-directive-mode none
)

..\..\..\dependencies\slang\windows\slangc.exe pbr.slang -target spirv -o pbr.vert.spv -entry VSMain %debug_flags%
..\..\..\dependencies\slang\windows\slangc.exe pbr.slang -target spirv -o pbr.frag.spv -entry FSMain %debug_flags%
..\..\..\dependencies\slang\windows\slangc.exe hzb.slang -target spirv -o hzb.comp.spv -entry CSMain %debug_flags%
..\..\..\dependencies\slang\windows\slangc.exe zprepass.slang -target spirv -o zprepass.vert.spv -entry VSMain %debug_flags%
..\..\..\dependencies\slang\windows\slangc.exe zprepass.slang -target spirv -o zprepass.frag.spv -entry FSMain %debug_flags%