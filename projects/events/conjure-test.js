project('events-tests', (_) => {
    kind('Executable');
    language('C++');

    when({ system: 'windows' }, (_) => {
        subsystem('Console');
    });

    files([
        './tests/**/*.hpp',
        './tests/**/*.cpp'
    ]);

    dependsOn([
        'ecs',
        'googletest',
    ]);

    uses([
        'tempest:common',
        'events:public',
        'googletest:public',
    ]);
});