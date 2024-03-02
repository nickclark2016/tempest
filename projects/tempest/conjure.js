project('tempest', (_) => {
    kind('StaticLib');
    language('C++');

    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    toolset('msc:143');

    externalWarnings('Off');

    block('tempest:engine', (_) => {
        includeDirs([
            './include'
        ]);

        uses([
            'tempest:common',
            'assets:public',
            'core:public',
            'ecs:public',
            'graphics:public',
            'logger:public',
            'math:public',
        ]);

        dependsOn([
            'assets',
            'core',
            'ecs',
            'logger',
            'graphics',
            'math',
        ]);
    });

    uses(['tempest:engine']);
});