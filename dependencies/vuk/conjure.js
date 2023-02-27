function find_vulkan_libs()
{
    const paths = process.env.PATH;
    const pathsList = paths.split(";").filter(path => path.includes('VulkanSDK'));
    if (pathsList.length === 0) {
        throw new Error('Failed to find Vulkan libraries.');
    }

    return `${pathsList[0]}/../Lib`
}

project('vuk', (prj) => {
    kind('StaticLib');
    language('C++');
    toolset('msc:143');
    staticRuntime('Off');

    files([
        'include/**/*.hpp',
        'src/**/*.cpp',
        'src/**/*.hpp',
    ]);

    block('vuk:public', (_) => {
        includeDirs([
            'include'
        ]);

        uses([
            'doctest:public',
            'concurrentqueue:public',
            'robinhood:public',
            'plf_colony:public',
            'spirv-cross:public',
            'vulkan:public',
        ]);
    });

    dependsOn([
        'doctest',
        'spirv-cross'
    ]);

    linksStatic([
        'vulkan-1.lib'
    ]);

    libraryDirs([
        find_vulkan_libs()
    ]);

    uses([ 'vuk:public' ]);

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