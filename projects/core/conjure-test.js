project('core-tests', (_) => {
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
        'googletest',
        'core',
    ]);

    uses([
        'tempest:common',
        'googletest:public',
        'core:public',
    ]);

    when({ system: 'linux' }, (_) => {
        linksStatic(['dl', 'X11', 'pthread']);
    });
});