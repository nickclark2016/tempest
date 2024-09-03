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
        'simdjson',
        'core',
        'ecs',
        'logger'
    ]);

    block('assets:public', (_) => {
        includeDirs([
            './include'
        ]);

        uses([
            'core:public',
            'ecs:public',
            'logger:public'
        ]);
    });

    uses([
        'tempest:common',
        'stb:public',
        'simdjson:public',
        'tinygltf:public',
        'assets:public',
        'math:public'
    ]);
});