project('sandbox', (_) => {
    kind('ConsoleApp');
    language('C++');

    files([
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    uses([
        'tempest:engine'
    ]);

    dependsOn(['tempest']);
});