#!/bin/sh

# Check for debug flag
if [ "$1" = "debug" ]; then
    DEBUG="-g3 -O1 -line-directive-mode source-map"
else
    DEBUG="-O3 -line-directive-mode none"
fi

# Compile shaders
../../../dependencies/slang/linux/slangc pbr.slang -target spirv -o pbr.vert.spv -entry VSMain $DEBUG && \
../../../dependencies/slang/linux/slangc pbr.slang -target spirv -o pbr.frag.spv -entry FSMain $DEBUG && \
echo "Compiled PBR shaders"