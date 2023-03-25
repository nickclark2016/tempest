project('tlsf', (prj) => {
    kind('StaticLib');
    language('C');
    toolset('msc:143');

    when({}, (ctx) => {
        targetDirectory(`${ctx.pathToWorkspace}/bin/${ctx.platform}/${ctx.configuration}`);
        intermediateDirectory(`${ctx.pathToWorkspace}/bin-int/${ctx.platform}/${ctx.configuration}/${prj.name}`);
    });

    files([
        'include/tlsf/tlsf.h',
        'src/tlsf.c',
    ]);

    staticRuntime('Off');

    includeDirs([
        'include/tlsf'
    ]);

    when({ configuration: 'Release' }, (_) => {
        optimize('Full');
        runtime('Release');
        symbols('Off');
    });

    when({ configuration: 'Debug' }, (_) => {
        optimize('Off');
        runtime('Debug');
        symbols('On')
    });

    block('tlsf:public', (_) => {
        includeDirs([
            './include'
        ]);
    });
});