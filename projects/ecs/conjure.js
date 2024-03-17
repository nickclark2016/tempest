project('ecs', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    block('ecs:public', (_) => {
        includeDirs([
            './include'
        ]);

        uses([
            'core:public',
            'math:public',
        ]);
    });

    uses([
        'tempest:common',
        'ecs:public'
    ]);
});