project('assets-tests', (_) => {
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
        'assets'
    ]);

    uses([
        'tempest:common',
        'googletest:public',
        'assets:public',
    ]);

    when({ system: 'linux' }, (_) => {
        linksStatic(['dl', 'X11', 'pthread']);
    });

    when({ system: 'windows' }, (_) => {
        defines(['_CRT_SECURE_NO_WARNINGS']);
    });
});