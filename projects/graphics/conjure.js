project('graphics', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    dependsOn([
        'core',
        'ecs',
        'glfw',
        'math',
        'logger',
        'vk-bootstrap',
        'vma',
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
        'core:public',
        'ecs:public',
        'glfw:public',
        'vk-bootstrap:public',
        'vma:public',
        'graphics:public',
        'logger:public',
        'math:public',
        'wyhash:public',
    ]);
});