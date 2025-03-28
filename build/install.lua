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

newaction {
    trigger = 'install',
    description = 'Install built application',
    execute = function()
        local installdir = _OPTIONS['installdir']
        local installcfg = _OPTIONS['installcfg']
        local installplatform = _OPTIONS['platform'] or 'x86_64'

        if not os.isdir(installdir) then
            os.mkdir(installdir)
        end

        -- Build the bin location
        local binDir = path.join(_MAIN_SCRIPT_DIR, 'bin', installcfg, _TARGET_OS, installplatform)
        
        -- Ensure the binaries have been built
        if not os.isdir(binDir) then
            error('Binaries have not been built - ' .. binDir)
        end

        if _TARGET_OS == 'linux' then
            -- Linux editor and GLFW so
            os.copyfile(path.join(binDir, 'editor'), path.join(installdir, 'editor'))
            os.copyfile(path.join(binDir, 'libglfw.so'), path.join(installdir, 'libglfw.so'))
        elseif _TARGET_OS == 'windows' then
            -- Windows editor and GLFW dll
            os.copyfile(path.join(binDir, 'editor.exe'), path.join(installdir, 'editor.exe'))
            os.copyfile(path.join(binDir, 'glfw.dll'), path.join(installdir, 'glfw.dll'))
        else
            error('Unsupported OS')
        end

        -- Build the assets directory
        local assetsDir = path.join(installdir, 'assets')
        if not os.isdir(assetsDir) then
            os.mkdir(assetsDir)
        end

        -- Make the shader directory and recursively copy the shaders
        local shaderDir = path.join(assetsDir, 'shaders')
        if not os.isdir(shaderDir) then
            os.mkdir(shaderDir)
        end

        local shaders = os.matchfiles(path.join(binDir, 'shaders', '**'))
        for _, shader in ipairs(shaders) do
            print('Copying ' .. shader)
            os.copyfile(shader, path.join(shaderDir, path.getname(shader)))
        end

        -- Symlink the models
        local gltfModelInstallDir = path.join(assetsDir, 'glTF-Sample-Assets')
        if not os.islink(gltfModelInstallDir) then
            local gltfModelDir = path.join(_MAIN_SCRIPT_DIR, 'vendor', 'glTF-Sample-Assets')
            os.linkdir(gltfModelDir, gltfModelInstallDir)
        end

        -- Symlink the polyhaven folder
        local polyhavenInstallDir = path.join(assetsDir, 'polyhaven')
        if not os.islink(polyhavenInstallDir) then
            local polyhavenDir = path.join(_MAIN_SCRIPT_DIR, 'vendor', 'polyhaven')
            os.linkdir(polyhavenDir, polyhavenInstallDir)
        end
    end
}