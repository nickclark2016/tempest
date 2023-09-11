project('core-tests', (prj) => {
    kind('ConsoleApp');
    language('C++');

    files([
        './tests/**/*.hpp',
        './tests/**/*.cpp'
    ]);

    toolset('msc:143');

    dependsOn([
        'googletest',
    ]);

    uses([
        'tempest:common',
        'googletest:public',
        'core:public',
    ]);
});