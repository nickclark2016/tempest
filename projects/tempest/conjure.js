project('tempest', (_) => {
    kind('StaticLib');
    language('C++');

    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

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
            'graphics',
            'assets',
            'core',
            'ecs',
            'logger',
            'math',
        ]);

        when({ system: 'linux' }, (_) => {
           linksStatic(['dl', 'X11', 'pthread']);
        });
    });

    uses(['tempest:engine']);
});