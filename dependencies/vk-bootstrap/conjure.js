project('vk-bootstrap', (prj) => {
    kind('StaticLib');
    language('C++');
    languageVersion('C++20');
    toolset('msc:143');
    staticRuntime('Off');

    files([
        'include/**.h',
        'src/**.cpp'
    ]);

    block('vk-bootstrap:public', (_) => {
        includeDirs([
            'include'
        ]);

        uses(['vulkan:public']);
    });

    uses([ 'vk-bootstrap:public' ]);

    when({}, (ctx) => {
        targetDirectory(`${ctx.pathToWorkspace}/bin/${ctx.platform}/${ctx.configuration}`);
        intermediateDirectory(`${ctx.pathToWorkspace}/bin-int/${ctx.platform}/${ctx.configuration}/${prj.name}`);
    });

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
});