project('simdjson', (prj) => {
    kind('StaticLib');
    language('C++');
    languageVersion('C++20');

    when({}, (ctx) => {
        targetDirectory(`${ctx.pathToWorkspace}/bin/${ctx.platform}/${ctx.configuration}`);
        intermediateDirectory(`${ctx.pathToWorkspace}/bin-int/${ctx.platform}/${ctx.configuration}/${prj.name}`);
    });

    files([
        'include/simdjson.h',
        'src/simdjson.cpp'
    ]);

    when({ configuration: 'Release' }, (_) => {
        optimize('Full');
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

    block('simdjson:public', (_) => {
        externalIncludeDirs([
            './include'
        ]);
    });

    includeDirs([
        './include'
    ]);

    when({ system: 'windows' }, (_) => {
        toolset('msc:143');
    });

    when({ system: 'linux' }, (_) => {
        toolset('clang');
    });

    when({ system: 'windows' }, (_) => {
        toolset('msc:143');
    });

    warnings('Off');
});