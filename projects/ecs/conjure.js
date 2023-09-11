project('ecs', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    toolset('msc:143');

    block('ecs:public', (_) => {
        includeDirs([
            './include'
        ]);

        uses([
            'core:public'
        ])
    });

    uses([
        'tempest:common',
        'ecs:public'
    ]);
});