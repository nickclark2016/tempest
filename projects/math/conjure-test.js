project('math-tests', (_) => {
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
    ]);

    uses([
        'tempest:common',
        'googletest:public',
        'math:public',
    ]);
});