project('vma', (prj) => {
    kind('StaticLib');
    language('C++');
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
        'VMA_STATIC_VULKAN_FUNCTIONS=1',
        // 'VMA_DYNAMIC_VULKAN_FUNCTIONS=1'
    ])

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

    block('vma:public', (_) => {
        includeDirs([
            './include'
        ]);
    });

    uses([
        'vulkan:public'
    ]);
});