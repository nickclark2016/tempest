project('editor', (_) => {
    kind('ConsoleApp');
    language('C++');

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
});