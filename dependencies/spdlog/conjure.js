project('spdlog', (prj) => {
    kind('StaticLib');
    language('C++');
    languageVersion('C++20');
    
    files([
        './src/spdlog.cpp',
    ]);

    toolset('msc:143');

    staticRuntime('Off');

    when({ configuration: 'Release' }, (filter) => {
        optimize('Speed');
        runtime('Release');
        symbols('Off');
        defines([
            'NDEBUG'
        ]);
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

    block('spdlog:public', (blk) => {
        includeDirs([
            './include'
        ]);
    });

    uses([
        'spdlog:public',
    ]);
});