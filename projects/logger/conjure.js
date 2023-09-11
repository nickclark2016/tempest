project('logger', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    toolset('msc:143');

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