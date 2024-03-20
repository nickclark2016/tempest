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
