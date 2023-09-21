project('vma', (prj) => {
    kind('StaticLib');
    language('C++');
    languageVersion('C++20');
    toolset('msc:143');

    when({}, (ctx) => {
        targetDirectory(`${ctx.pathToWorkspace}/bin/${ctx.platform}/${ctx.configuration}`);
        intermediateDirectory(`${ctx.pathToWorkspace}/bin-int/${ctx.platform}/${ctx.configuration}/${prj.name}`);
    });

    files([
        'include/vk_mem_alloc.h',
        'src/Common.cpp',
        'src/Common.h',
        'src/VmaUsage.cpp',
        'src/VmaUsage.h'
    ]);

    staticRuntime('Off');

    defines([
        "VMA_STATIC_VULKAN_FUNCTIONS=0",
        "VMA_DYNAMIC_VULKAN_FUNCTIONS=1"
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

    block('vma:public', (_) => {
        externalIncludeDirs([
            './include'
        ]);
    });

    uses([
        'vulkan:public'
    ]);

    warnings('Off');
});