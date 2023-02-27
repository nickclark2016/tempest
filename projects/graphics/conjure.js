project('graphics', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    dependsOn([
        'glfw',
        'logger',
        'vk-bootstrap',
        'vma',
        'vuk',
    ]);

    toolset('msc:143');

    externalWarnings('Off');

    when({}, (ctx) => {
        targetDirectory(`${ctx.pathToWorkspace}/bin/${ctx.platform}/${ctx.configuration}`);
        intermediateDirectory(`${ctx.pathToWorkspace}/bin-int/${ctx.platform}/${ctx.configuration}/${prj.name}`);
    });

    when({ configuration: 'Debug' }, (ctx) => {
        defines([
            '_DEBUG'
        ]);
    });

    block('graphics:public', (_) => {
        includeDirs([
            './include'
        ]);
    });

    uses([
        'glfw:public',
        'vk-bootstrap:public',
        'vma:public',
        'vuk:public',
        'graphics:public',
        'logger:public',
    ]);
});