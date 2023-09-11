project('input', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    toolset('msc:143');

    dependsOn([
        'glfw'
    ]);

    block('input:public', (_) => {
        includeDirs([
            './include'
        ]);
    });

    uses([
        'tempest:common',
        'glfw:public',
        'input:public'
    ]);
});