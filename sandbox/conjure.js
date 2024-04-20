project('sandbox', _ => {
    kind('ConsoleApp');
    language('C++');

    files([
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    uses([
        'tempest:engine'
    ]);

    when({}, (ctx) => {
        const slangPath = (() => {
            if (ctx.system === 'windows') {
                return `./dependencies/slang/${ctx.system}/slangc.exe`;
            } else if (ctx.system === 'linux') {
                return `./dependencies/slang/${ctx.system}/slangc`;
            } else {
                throw new Error(`Unsupported system: ${ctx.system}`);
            }
        })();

        const shaderBasePath = 'sandbox/assets/shaders';
        const debugFlags = ctx.configuration === 'Debug' ? '-g3 -O1 -line-directive-mode source-map' : '-O3';

        const events = [
            `${slangPath} ${shaderBasePath}/pbr.slang -target spirv -o ${shaderBasePath}/pbr.vert.spv -entry VSMain ${debugFlags}`,
            `${slangPath} ${shaderBasePath}/pbr.slang -target spirv -o ${shaderBasePath}/pbr.frag.spv -entry FSMain ${debugFlags}`,
            `${slangPath} ${shaderBasePath}/hzb.slang -target spirv -o ${shaderBasePath}/hzb.comp.spv -entry CSMain ${debugFlags}`,
            `${slangPath} ${shaderBasePath}/zprepass.slang -target spirv -o ${shaderBasePath}/zprepass.vert.spv -entry VSMain ${debugFlags}`,
            `${slangPath} ${shaderBasePath}/zprepass.slang -target spirv -o ${shaderBasePath}/zprepass.frag.spv -entry FSMain ${debugFlags}`,
            `${slangPath} ${shaderBasePath}/taa.slang -target spirv -o ${shaderBasePath}/taa.vert.spv -entry VSMain ${debugFlags}`,
            `${slangPath} ${shaderBasePath}/taa.slang -target spirv -o ${shaderBasePath}/taa.frag.spv -entry FSMain ${debugFlags}`,
            `${slangPath} ${shaderBasePath}/sharpen.slang -target spirv -o ${shaderBasePath}/sharpen.vert.spv -entry VSMain ${debugFlags}`,
            `${slangPath} ${shaderBasePath}/sharpen.slang -target spirv -o ${shaderBasePath}/sharpen.frag.spv -entry FSMain ${debugFlags}`
        ];

        for (let i = 0; i < events.length; i++) {
            events[i] = events[i].replaceAll('/', '\\');
        }

        postBuildEvents(events);
    });

    dependsOn(['tempest']);
});