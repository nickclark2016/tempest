project('sandbox', (_) => {
    kind('ConsoleApp');
    language('C++');

    files([
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    toolset('msc:143');

    uses([
        'tempest:engine'
    ]);
});