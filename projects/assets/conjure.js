project('assets', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    toolset('msc:143');

    dependsOn([
        'tinygltf',
        'core',
        'logger'
    ]);

    block('assets:public', (_) => {
        includeDirs([
            './include'
        ]);

        uses([
            'core:public',
            'logger:public'
        ]);
    });

    uses([
        'tempest:common',
        'tinygltf:public',
        'assets:public',
        'math:public'
    ]);
});