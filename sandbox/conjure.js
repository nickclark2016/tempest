project('sandbox', (prj) => {
    kind('ConsoleApp');
    language('C++');

    files([
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    toolset('msc:143');

    when({}, (ctx) => {
        targetDirectory(`${ctx.pathToWorkspace}/bin/${ctx.platform}/${ctx.configuration}`);
        intermediateDirectory(`${ctx.pathToWorkspace}/bin-int/${ctx.platform}/${ctx.configuration}/${prj.name}`);
    });

    uses([
        'tempest:engine'
    ]);
});