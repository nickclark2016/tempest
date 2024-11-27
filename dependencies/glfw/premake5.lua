project 'glfw'
    kind 'SharedLib'
    language 'C'
    cdialect 'C11'
    targetname 'glfw'
    
    targetdir '%{binaries}'
    objdir '%{intermidates}'

    files {
        'include/GLFW/glfw3.h',
        'include/GLFW/glfw3native.h',
        'src/glfw_config.h',
        'src/context.c',
        'src/init.c',
        'src/input.c',
        'src/monitor.c',
        'src/vulkan.c',
        'src/window.c'
    }

    defines {
        '_GLFW_BUILD_DLL',
    }

    filter { 'system:windows' }
        systemversion 'latest'
        staticruntime 'Off'

        files {
            'src/egl_context.c',
            'src/osmesa_context.c',
            'src/wgl_context.c',
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
        }

        defines {
            '_GLFW_WIN32',
            '_CRT_SECURE_NO_WARNINGS',
        }

    filter { 'system:linux' }
        files {
            'src/glx_context.c',
            'src/linux_joystick.c',
            'src/posix_thread.c',
            'src/posix_thread.h',
            'src/posix_time.c',
            'src/posix_time.h',
            'src/x11_init.c',
            'src/x11_monitor.c',
            'src/x11_window.c',
            'src/xkb_unicode.c',
        }

        defines {
            '_GLFW_X11',
            'BUILD_SHARED_LIBS',
        }

        pic 'On'

    filter {}

    IncludeDir['glfw'] = '%{root}/dependencies/glfw/include'