include('./core/conjure.js');
include('./ecs/conjure.js');
include('./graphics/conjure.js');
include('./logger/conjure.js');
include('./math/conjure.js');
include('./assets/conjure.js');

block('tempest:engine', (_) => {
    uses([
        'tempest:common',
        'core:public',
        'ecs:public',
        'graphics:public',
        'logger:public',
        'math:public',
        'assets:public'
    ]);

    dependsOn([
        'core',
        'ecs',
        'graphics',
        'logger',
        'math',
        'assets'
    ]);
});