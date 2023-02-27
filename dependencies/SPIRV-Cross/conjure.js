project('spirv-cross', (prj) => {
    kind('StaticLib');
    language('C++');
    toolset('msc:143');
    staticRuntime('Off');

    files([
        'include/**/*.h',
        'include/**/*.hpp',
        'src/**/*.cpp',
    ]);

    block('spirv-cross:public', (_) => {
        includeDirs([
            './include'
        ]);

        uses([
            'vk-bootstrap:public',
            'vma:public'
        ]);
    });

    uses([ 'spirv-cross:public' ]);

    includeDirs([
        './include/spirv_cross',
    ]);

    warnings('Off');

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