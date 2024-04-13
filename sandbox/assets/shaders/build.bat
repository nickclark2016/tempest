@echo off

:: debug flags: -g3 -O1 -line-directive-mode source-map

:: check for debug flag
IF /i "%1" == "debug" goto debug
IF /i "%1" == "rel_with_symbols" goto release_with_symbols
goto release

:debug
set debug_flags=-g -O1 -line-directive-mode source-map
goto compile

:release_with_symbols
set debug_flags=-g -O3 -line-directive-mode source-map
goto compile

:release
set debug_flags=-O3 -line-directive-mode none
goto compile

:compile
..\..\..\dependencies\slang\windows\slangc.exe pbr.slang -target spirv -o pbr.vert.spv -entry VSMain %debug_flags%
..\..\..\dependencies\slang\windows\slangc.exe pbr.slang -target spirv -o pbr.frag.spv -entry FSMain %debug_flags%
..\..\..\dependencies\slang\windows\slangc.exe hzb.slang -target spirv -o hzb.comp.spv -entry CSMain %debug_flags%
..\..\..\dependencies\slang\windows\slangc.exe zprepass.slang -target spirv -o zprepass.vert.spv -entry VSMain %debug_flags%
..\..\..\dependencies\slang\windows\slangc.exe zprepass.slang -target spirv -o zprepass.frag.spv -entry FSMain %debug_flags%
..\..\..\dependencies\slang\windows\slangc.exe taa.slang -target spirv -o taa.vert.spv -entry VSMain %debug_flags%
..\..\..\dependencies\slang\windows\slangc.exe taa.slang -target spirv -o taa.frag.spv -entry FSMain %debug_flags%