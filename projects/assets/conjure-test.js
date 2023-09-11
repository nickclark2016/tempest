project('assets-tests', (prj) => {
    kind('ConsoleApp');
    language('C++');

    files([
        './tests/**/*.hpp',
        './tests/**/*.cpp'
    ]);

    toolset('msc:143');

    dependsOn([
        'googletest',
        'assets'
    ]);

    uses([
        'tempest:common',
        'googletest:public',
        'assets:public',
    ]);
});