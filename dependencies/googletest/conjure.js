project('googletest', (prj) => {
    kind('StaticLib');
    language('C++');
    languageVersion('C++20');
    
    files([
        './src/gmock-all.cc',
        './src/gtest-all.cc'
    ]);

    includeDirs([
        '.',
        './include'
    ]);

    staticRuntime('Off');

    when({ configuration: 'Release' }, (_) => {
        optimize('Speed');
        runtime('Release');
        symbols('Off');
        defines([
            'NDEBUG'
        ]);
    });

    when({ configuration: 'Debug' }, (_) => {
        optimize('Off');
        runtime('Debug');
        symbols('On');
        defines([
            '_DEBUG'
        ]);
    });

    when({}, (ctx) => {
        targetDirectory(`${ctx.pathToWorkspace}/bin/${ctx.platform}/${ctx.configuration}`);
        intermediateDirectory(`${ctx.pathToWorkspace}/bin-int/${ctx.platform}/${ctx.configuration}/${prj.name}`);
    });

    block('googletest:public', (_) => {
        externalIncludeDirs([
            './include'
        ]);

        externalWarnings('Off');
    });

    when({ system: 'windows' }, () => {
        toolset('msc:143');
    });

    when({ system: 'linux' }, () => {
        toolset('clang');
    });
});