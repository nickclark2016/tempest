project('logger', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    toolset('msc:143');

    dependsOn([
        'spdlog',
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

    block('logger:public', (_) => {
        includeDirs([
            './include'
        ]);
    });

    uses([
        'logger:public',
        'spdlog:public'
    ]);
});