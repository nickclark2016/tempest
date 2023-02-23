project('input', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**.hpp',
        './src/**.hpp',
        './src/**.cpp'
    ]);

    toolset('msc:143');

    dependsOn([
        'glfw'
    ]);

    when({}, (ctx) => {
        targetDirectory(`${ctx.pathToWorkspace}/bin/${ctx.platform}/${ctx.configuration}`);
        intermediateDirectory(`${ctx.pathToWorkspace}/bin-int/${ctx.platform}/${ctx.configuration}/${prj.name}`);
    });

    when({ configuration: 'Debug' }, (ctx) => {
        defines([
            '_DEBUG'
        ]);
    });

    block('input:public', (_) => {
        includeDirs([
            './include'
        ]);
    });

    uses([
        'glfw:public',
        'input:public'
    ]);
});