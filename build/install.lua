newoption {
    trigger = 'installdir',
    value = 'string',
    description = 'Directory to install the application to.',
    default = './install',
    category = 'Tempest Engine',
}

newoption {
    trigger = 'installcfg',
    value = 'string',
    description = 'Configuration of the build to install.',
    default = 'Release',
    category = 'Tempest Engine',
}

newoption {
    trigger = 'installtoolset',
    value = 'string',
    description = 'Toolset of the build to install (e.g. clang, msc-v143, msc-v145).',
    category = 'Tempest Engine',
}

newaction {
    trigger = 'install',
    description = 'Install built application',
    execute = function()
        local installdir = _OPTIONS['installdir']
        local installcfg = _OPTIONS['installcfg']
        local installplatform = _OPTIONS['platform'] or 'x86_64'
        local explicitToolset = _OPTIONS['installtoolset'] or _OPTIONS['cc']

        if not os.isdir(installdir) then
            os.mkdir(installdir)
        end

        -- Resolve binary directory matching premake5.lua layout: bin/<installcfg>/<target_os>-<toolset>
        local configDir = path.join(_MAIN_SCRIPT_DIR, 'bin', installcfg)
        local candidateDirs = {}

        if explicitToolset then
            if explicitToolset:startswith(_TARGET_OS .. '-') then
                table.insert(candidateDirs, path.join(configDir, explicitToolset))
            else
                table.insert(candidateDirs, path.join(configDir, _TARGET_OS .. '-' .. explicitToolset))
            end
        end

        if _TARGET_OS == 'windows' then
            table.insert(candidateDirs, path.join(configDir, 'windows-clang'))
            table.insert(candidateDirs, path.join(configDir, 'windows-msc-v143'))
            table.insert(candidateDirs, path.join(configDir, 'windows-msc-v145'))
        elseif _TARGET_OS == 'linux' then
            table.insert(candidateDirs, path.join(configDir, 'linux-clang'))
            table.insert(candidateDirs, path.join(configDir, 'linux-gcc'))
        end

        if os.isdir(configDir) then
            local matches = os.matchdirs(path.join(configDir, _TARGET_OS .. '-*'))
            for _, m in ipairs(matches) do
                table.insert(candidateDirs, m)
            end
        end

        table.insert(candidateDirs, path.join(configDir, _TARGET_OS, installplatform))

        local binDir = nil
        for _, candidate in ipairs(candidateDirs) do
            if os.isdir(candidate) then
                binDir = candidate
                break
            end
        end

        if not binDir then
            error('Binaries have not been built for configuration "' .. installcfg .. '". Searched:\n  ' .. table.concat(candidateDirs, '\n  '))
        end

        -- Copy editor executable
        local editorExeName = _TARGET_OS == 'windows' and 'editor-entrypoint.exe' or 'editor-entrypoint'
        local legacyExeName = _TARGET_OS == 'windows' and 'editor.exe' or 'editor'
        local editorFound = false

        if os.isfile(path.join(binDir, editorExeName)) then
            os.copyfile(path.join(binDir, editorExeName), path.join(installdir, editorExeName))
            os.copyfile(path.join(binDir, editorExeName), path.join(installdir, legacyExeName))
            editorFound = true
        elseif os.isfile(path.join(binDir, legacyExeName)) then
            os.copyfile(path.join(binDir, legacyExeName), path.join(installdir, legacyExeName))
            editorFound = true
        end

        if not editorFound then
            error('Editor executable (' .. editorExeName .. ' or ' .. legacyExeName .. ') not found in ' .. binDir)
        end

        -- Copy runner executable if present
        local runnerExeName = _TARGET_OS == 'windows' and 'runner.exe' or 'runner'
        if os.isfile(path.join(binDir, runnerExeName)) then
            os.copyfile(path.join(binDir, runnerExeName), path.join(installdir, runnerExeName))
        end

        -- Copy runtime shared libraries / dynamic link libraries
        if _TARGET_OS == 'windows' then
            local dlls = os.matchfiles(path.join(binDir, '*.dll'))
            for _, dll in ipairs(dlls) do
                os.copyfile(dll, path.join(installdir, path.getname(dll)))
            end
        elseif _TARGET_OS == 'linux' then
            local sos = os.matchfiles(path.join(binDir, '*.so*'))
            for _, so in ipairs(sos) do
                os.copyfile(so, path.join(installdir, path.getname(so)))
            end
        end

        -- Build the assets directory
        local assetsDir = path.join(installdir, 'assets')
        if not os.isdir(assetsDir) then
            os.mkdir(assetsDir)
        end

        -- Make the shader directory and recursively copy shaders preserving subdirectory structure
        local shaderDestDir = path.join(assetsDir, 'shaders')
        if not os.isdir(shaderDestDir) then
            os.mkdir(shaderDestDir)
        end

        local shaderSources = {}
        local binShaderDir = path.join(binDir, 'shaders')
        if os.isdir(binShaderDir) then
            table.insert(shaderSources, binShaderDir)
        end
        local repoShaderDir = path.join(_MAIN_SCRIPT_DIR, 'assets', 'shaders')
        if os.isdir(repoShaderDir) then
            table.insert(shaderSources, repoShaderDir)
        end

        for _, srcRoot in ipairs(shaderSources) do
            local shaders = os.matchfiles(path.join(srcRoot, '**'))
            for _, shader in ipairs(shaders) do
                if os.isfile(shader) then
                    local relPath = path.getrelative(srcRoot, shader)
                    local destPath = path.join(shaderDestDir, relPath)
                    local destParent = path.getdirectory(destPath)
                    if not os.isdir(destParent) then
                        os.mkdir(destParent)
                    end
                    print('Copying ' .. shader .. ' -> ' .. destPath)
                    os.copyfile(shader, destPath)
                end
            end
        end

        -- Symlink the models
        local gltfModelInstallDir = path.join(assetsDir, 'glTF-Sample-Assets')
        if not os.islink(gltfModelInstallDir) and not os.isdir(gltfModelInstallDir) then
            local gltfModelDir = path.join(_MAIN_SCRIPT_DIR, 'vendor', 'glTF-Sample-Assets')
            if os.isdir(gltfModelDir) then
                os.linkdir(gltfModelDir, gltfModelInstallDir)
            end
        end

        -- Symlink the polyhaven folder
        local polyhavenInstallDir = path.join(assetsDir, 'polyhaven')
        if not os.islink(polyhavenInstallDir) and not os.isdir(polyhavenInstallDir) then
            local polyhavenDir = path.join(_MAIN_SCRIPT_DIR, 'vendor', 'polyhaven')
            if os.isdir(polyhavenDir) then
                os.linkdir(polyhavenDir, polyhavenInstallDir)
            end
        end
    end
}

newoption {
    trigger = 'enable-aftermath',
    description = 'Enable NSight Aftermath support for Vulkan. Windows only.',
    category = 'Tempest Engine',
}