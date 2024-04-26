workspace('Tempest', (_) => {
    platforms(['x64']);
    configurations(['Debug', 'Release']);

    group('Engine', (_) => {
        include('./projects/conjure.js');
    });

    group('Tests', (_) => {
        include('./projects/conjure-test.js');
    });

    include('./sandbox/conjure.js');

    group('Dependencies', (_) => {
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

        when({ configuration: 'Release' }, (_) => {
            optimize('Full');
            runtime('Release');
            symbols('Off');
            defines([
                'NDEBUG'
            ]);
        });

        when({ system: 'windows' }, () => {
            toolset('msc:143');
        });

        when({ system: 'linux' }, () => {
            toolset('clang');
        });

        when({}, (ctx) => {
            targetDirectory(`${ctx.pathToWorkspace}/bin/${ctx.platform}/${ctx.configuration}`);
            intermediateDirectory(`${ctx.pathToWorkspace}/bin-int/${ctx.platform}/${ctx.configuration}/${ctx.project.getName()}`);
        });

        languageVersion('C++20');
        externalWarnings('Off');
    });
});

onConfigure(() => {
    when({ system: 'windows' }, (_) => {
        fetchRemoteZip({
            url: 'https://github.com/shader-slang/slang/releases/download/v2024.1.6/slang-2024.1.6-win64.zip',
            files: [
                'bin/windows-x64/release/slangc.exe',
                'bin/windows-x64/release/gfx.dll',
                'bin/windows-x64/release/slang.dll',
                'bin/windows-x64/release/slang-glslang.dll',
                'bin/windows-x64/release/slang-llvm.dll',
                'bin/windows-x64/release/slang-rt.dll'
            ],
            destination: './dependencies/slang/windows'
        });
    });

    when({ system: 'linux' }, (_) => {
        fetchRemoteZip({
            url: 'https://github.com/shader-slang/slang/releases/download/v2024.1.6/slang-2024.1.6-linux-x86_64.zip',
            files: [
                'bin/linux-x64/release/slangc',
                'bin/linux-x64/release/libgfx.so',
                'bin/linux-x64/release/libslang.so',
                'bin/linux-x64/release/libslang-glslang.so',
                'bin/linux-x64/release/libslang-llvm.so'
            ],
            destination: './dependencies/slang/linux'
        });
    });
});