workspace('Tempest', (wks) => {
    platforms(['x64']);
    configurations(['Debug', 'Release']);

    group('Engine', (grp) => {
        include('./projects/conjure.js');
    });

    group('Tests', (grp) => {
        include('./projects/conjure-test.js');
    });

    include('./sandbox/conjure.js');

    group('Dependencies', (grp) => {
        include('./dependencies/conjure.js');
    });

    block('tempest:common', (_) => {
        when({ configuration: 'Debug' }, (_) => {
            optimize('Off');
            runtime('Debug');
            symbols('On');
            defines([
                '_DEBUG'
            ]);
        });
    
        when({ configuration: 'Release'}, (_) => {
            optimize('Full');
            runtime('Release');
            symbols('Off');
            defines([
                'NDEBUG'
            ]);
        });

        when({}, (ctx) => {
            targetDirectory(`${ctx.pathToWorkspace}/bin/${ctx.platform}/${ctx.configuration}`);
            intermediateDirectory(`${ctx.pathToWorkspace}/bin-int/${ctx.platform}/${ctx.configuration}/${ctx.project.getName()}`);
        });
    });
});