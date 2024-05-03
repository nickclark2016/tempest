project('core-tests', (prj) => {
    kind('ConsoleApp');
    language('C++');

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
});