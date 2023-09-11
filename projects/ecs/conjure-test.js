project('ecs-tests', (prj) => {
    kind('ConsoleApp');
    language('C++');

    files([
        './tests/**/*.hpp',
        './tests/**/*.cpp'
    ]);

    toolset('msc:143');

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