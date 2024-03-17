project('assets', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

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
        'stb:public',
        'tinygltf:public',
        'assets:public',
        'math:public'
    ]);
});