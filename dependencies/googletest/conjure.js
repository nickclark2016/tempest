project('googletest', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './src/gmock-all.cc',
        './src/gtest-all.cc'
    ]);

    includeDirs([
        '.',
        './include'
    ]);

    toolset('msc:143');

    staticRuntime('Off');

    when({ configuration: 'Release' }, (filter) => {
        optimize('Speed');
        runtime('Release');
        symbols('Off');
    });

    when({ configuration: 'Debug' }, (filter) => {
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

    block('googletest:public', (blk) => {
        includeDirs([
            '.',
            './include'
        ]);
    });
});