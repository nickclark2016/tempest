project('sandbox', (prj) => {
    kind('ConsoleApp');
    language('C++');

    files([
        './src/**.cppm'
    ]);

    toolset('msc:143');

    dependsOn([
        'ecs',
        'graphics',
        'input',
        'math'
    ]);

    uses([
        'ecs:public',
        'graphics:public',
        'input:public',
        'math:public'
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
});