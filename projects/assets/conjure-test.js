project('assets-tests', (prj) => {
    kind('ConsoleApp');
    language('C++');

    files([
        './tests/**/*.hpp',
        './tests/**/*.cpp'
    ]);

    dependsOn([
        'googletest',
        'assets'
    ]);

    uses([
        'tempest:common',
        'googletest:public',
        'assets:public',
    ]);

    when('system:linux', (_) => {
        linksStatic(['dl', 'X11', 'pthread']);
     });
});