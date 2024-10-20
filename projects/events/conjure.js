project('events', (_) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    block('events:public', (_) => {
        includeDirs([
            './include'
        ]);

        uses([
            'core:public',
        ]);

        dependsOn([
            'core',
        ]);
    });

    uses([
        'tempest:common',
        'events:public'
    ]);
});