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
});