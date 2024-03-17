project('glfw', (prj) => {
    kind('SharedLib');
    language('C');
    targetName('glfw');

    when({}, (ctx) => {
        targetDirectory(`${ctx.pathToWorkspace}/bin/${ctx.platform}/${ctx.configuration}`);
        intermediateDirectory(`${ctx.pathToWorkspace}/bin-int/${ctx.platform}/${ctx.configuration}/${prj.name}`);
    });

    files([
        'include/GLFW/glfw3.h',
        'include/GLFW/glfw3native.h',
        'src/glfw_config.h',
        'src/context.c',
        'src/egl_context.c',
        'src/init.c',
        'src/input.c',
        'src/internal.h',
        'src/mappings.h',
        'src/monitor.c',
        'src/osmesa_context.c',
        'src/vulkan.c',
        'src/window.c',
    ]);

    when({ system: 'windows' }, (_) => {
        files([
            'src/win32_init.c',
            'src/win32_joystick.c',
            'src/win32_joystick.h',
            'src/win32_monitor.c',
            'src/win32_platform.h',
            'src/win32_time.c',
            'src/win32_time.h',
            'src/win32_thread.c',
            'src/win32_thread.h',
            'src/win32_window.c',
            'src/wgl_context.c',
        ]);

        defines([
            '_GLFW_BUILD_DLL',
            '_GLFW_WIN32',
            '_CRT_SECURE_NO_WARNINGS'
        ]);

        toolset('msc:143');
    });

    when('system:linux', (_) => {
        files([
            'src/glx_context.c',
            'src/linux_joystick.c',
            'src/posix_thread.c',
            'src/posix_time.c',
            'src/x11_init.c',
            'src/x11_monitor.c',
            'src/x11_window.c',
            'src/xkb_unicode.c',
        ]);

        defines([
            '_GLFW_X11',
            '_GLFW_BUILD_DLL',
            'BUILD_SHARED_LIBS'
        ]);

        toolset('clang');
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

    block('glfw:public', (blk) => {
        externalIncludeDirs([
            './include'
        ]);

        defines([
            'GLFW_DLL'
        ]);
    });
});