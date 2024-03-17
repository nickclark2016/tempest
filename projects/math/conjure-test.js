project('math-tests', (prj) => {
    kind('ConsoleApp');
    language('C++');

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