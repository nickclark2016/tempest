project('imgui', (prj) => {
    kind('StaticLib');
    language('C++');
    languageVersion('C++20');

    when({}, (ctx) => {
        targetDirectory(`${ctx.pathToWorkspace}/bin/${ctx.platform}/${ctx.configuration}`);
        intermediateDirectory(`${ctx.pathToWorkspace}/bin-int/${ctx.platform}/${ctx.configuration}/${prj.name}`);
    });

    files([
        'include/**',
        'src/**'
    ]);

    dependsOn([
        'glfw',
    ]);

    staticRuntime('Off');

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

    block('imgui:public', (_) => {
        externalIncludeDirs([
            './include'
        ]);

        defines([
            'IMGUI_IMPL_VULKAN_NO_PROTOTYPES'
        ]);
    });

    uses([
        'glfw:public',
        'imgui:public',
        'vulkan:public',
    ]);

    when('system:windows', () => {
        toolset('msc:143');
    });

    when('system:linux', () => {
        toolset('clang');
    });

    warnings('Off');
});