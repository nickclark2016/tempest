project('core', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    toolset('msc:143');

    dependsOn([
        'math',
        'tlsf',
        'glfw',
    ])

    block('core:public', (_) => {
        includeDirs([
            './include'
        ]);
    });

    uses([
        'tempest:common',
        'glfw:public',
        'tlsf:public',
        'core:public',
        'math:public',
    ]);
});