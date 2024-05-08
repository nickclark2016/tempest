project('editor', (prj) => {
    kind('Executable');
    language('C++');

    when({ system: 'windows' }, (_) => {
        subsystem('Console');
    });

    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    includeDirs([
        './include',
    ]);

    dependsOn([
        'tempest',
    ]);

    uses(['tempest:engine']);

    debugDirectory(`${prj.pathToWorkspace}/sandbox`);
});