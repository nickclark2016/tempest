project('ecs-tests', (prj) => {
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
        'ecs:public',
        'googletest:public',
    ]);
});