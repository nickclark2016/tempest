project('fmt', (prj) => {
    kind('StaticLib');
    language('C++');
    toolset('msc:143');
    staticRuntime('Off');

    files([
        'include/**/*.h',
        'src/**.cc'
    ]);

    block('fmt:public', (_) => {
        includeDirs([
            './include'
        ]);
    });

    uses([ 'fmt:public' ]);

    when({}, (ctx) => {
        targetDirectory(`${ctx.pathToWorkspace}/bin/${ctx.platform}/${ctx.configuration}`);
        intermediateDirectory(`${ctx.pathToWorkspace}/bin-int/${ctx.platform}/${ctx.configuration}/${prj.name}`);
    });

    when({ configuration: 'Release' }, (_) => {
        optimize('Full');
        runtime('Release');
        symbols('Off');
    });

    when({ configuration: 'Debug' }, (_) => {
        optimize('Off');
        runtime('Debug');
        symbols('On');
        defines([
            '_DEBUG'
        ]);
    });
});