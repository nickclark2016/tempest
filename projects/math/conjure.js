project('math', (prj) => {
    kind('StaticLib');
    language('C++');
    
    files([
        './include/**/*.hpp',
        './src/**/*.hpp',
        './src/**/*.cpp'
    ]);

    block('math:public', (_) => {
        includeDirs([
            './include'
        ]);
    });

    uses([
        'tempest:common',
        'math:public'
    ]);
});