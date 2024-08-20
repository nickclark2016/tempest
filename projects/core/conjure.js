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
        'glfw',
        'math',
        'tlsf',
    ])

    block('core:public', (_) => {
        includeDirs([
            './include'
        ]);

        when({ toolset: 'msc:143' }, (_) => {
            natvisFiles([
                './natvis/**/*.natvis',
            ]);
        });
    });

    uses([
        'tempest:common',
        'tlsf:public',
        'core:public',
        'math:public',
        'glfw:public',
    ]);
});