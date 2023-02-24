include('./ecs/conjure.js');
include('./graphics/conjure.js');
include('./input/conjure.js');
include('./logger/conjure.js');
include('./math/conjure.js');

block('tempest:engine', (_) => {
    uses([
        'ecs:public',
        'graphics:public',
        'input:public',
        'logger:public',
        'math:public'
    ]);

    dependsOn([
        'ecs',
        'graphics',
        'input',
        'logger',
        'math'
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