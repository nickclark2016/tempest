project('logger', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    dependsOn([
        'spdlog',
    ]);

    block('logger:public', (_) => {
        includeDirs([
            './include'
        ]);
    });

    uses([
        'tempest:common',
        'logger:public',
        'spdlog:public'
    ]);
});