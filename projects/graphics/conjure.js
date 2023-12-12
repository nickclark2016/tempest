project('graphics', (_) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    dependsOn([
        'core',
        'ecs',
        'glfw',
        'imgui',
        'math',
        'logger',
        'vk-bootstrap',
        'vma',
    ]);

    toolset('msc:143');

    externalWarnings('Off');

    block('graphics:public', (_) => {
        includeDirs([
            './include'
        ]);
    });

    uses([
        'tempest:common',
        'core:public',
        'ecs:public',
        'glfw:public',
        'graphics:public',
        'imgui:public',
        'logger:public',
        'math:public',
        'vk-bootstrap:public',
        'vma:public',
        'wyhash:public',
    ]);
});