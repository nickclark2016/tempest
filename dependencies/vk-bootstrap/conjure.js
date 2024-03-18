project('vk-bootstrap', (prj) => {
    kind('StaticLib');
    language('C++');
    languageVersion('C++20');
    staticRuntime('Off');

    files([
        'include/**.h',
        'src/**.cpp'
    ]);

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

    includeDirs([
        'include'
    ]);

    uses(['vulkan:public']);

    block('vk-bootstrap:public', (_) => {
        externalIncludeDirs([
            'include'
        ]);

        uses(['vulkan:public']);
    });

    when({ system: 'windows' }, () => {
        toolset('msc:143');
    });

    when({ system: 'linux' }, () => {
        toolset('clang');
    });
});