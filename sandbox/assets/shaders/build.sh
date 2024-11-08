#!/bin/sh

# Check for debug flag
if [ "$1" = "debug" ]; then
    DEBUG="-g3 -O1 -line-directive-mode source-map"
else if [ "$1" = "rel_with_symbols" ]; then
    DEBUG="-O3 -line-directive-mode none"
else
    DEBUG="-O3 -line-directive-mode none"
fi

# Compile shaders
../../../dependencies/slang/linux/slangc pbr.slang -target spirv -o pbr.vert.spv -entry VSMain $DEBUG && \
../../../dependencies/slang/linux/slangc pbr.slang -target spirv -o pbr.frag.spv -entry FSMain $DEBUG && \
../../../dependencies/slang/linux/slangc hzb.slang -target spirv -o hzb.comp.spv -entry CSMain $DEBUG && \
../../../dependencies/slang/linux/slangc zprepass.slang -target spirv -o zprepass.vert.spv -entry VSMain $DEBUG && \
../../../dependencies/slang/linux/slangc zprepass.slang -target spirv -o zprepass.frag.spv -entry FSMain $DEBUG && \
../../../dependencies/slang/linux/slangc taa.slang -target spirv -o taa.vert.spv -entry VSMain $DEBUG && \
../../../dependencies/slang/linux/slangc taa.slang -target spirv -o taa.frag.spv -entry FSMain $DEBUG && \
../../../dependencies/slang/linux/slangc directional_shadow_map.slang -target spirv -o directional_shadow_map.vert.spv -entry VSMain $DEBUG && \
../../../dependencies/slang/linux/slangc directional_shadow_map.slang -target spirv -o directional_shadow_map.frag.spv -entry FSMain $DEBUG && \
echo "Compiled PBR shaders"