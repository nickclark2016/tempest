include('./core/conjure.js');
include('./ecs/conjure.js');
include('./graphics/conjure.js');
include('./input/conjure.js');
include('./logger/conjure.js');
include('./math/conjure.js');
include('./assets/conjure.js');

block('tempest:engine', (_) => {
    uses([
        'core:public',
        'ecs:public',
        'graphics:public',
        'input:public',
        'logger:public',
        'math:public',
        'assets:public'
    ]);

    dependsOn([
        'core',
        'ecs',
        'graphics',
        'input',
        'logger',
        'math',
        'assets'
    ]);

    when({ configuration: 'Debug' }, (ctx) => {
        defines([
            '_DEBUG'
        ]);
    });

    when({ configuration: 'Release'}, (ctx) => {
        defines([
            'NDEBUG'
        ]);
    });
});