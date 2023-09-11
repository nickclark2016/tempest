project('graphics', (prj) => {
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
        'vk-bootstrap:public',
        'vma:public',
        'graphics:public',
        'logger:public',
        'math:public',
        'wyhash:public',
    ]);
});