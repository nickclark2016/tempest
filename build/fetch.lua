local function fetchIfNotExists(url, filename)
    if not os.isfile(filename) then
        print('Fetching ' .. filename)
        local res, response = http.download(url, filename)
        return response == 200
    end
    return false
end

newaction {
    trigger = 'fetch',
    description = 'Fetch binary dependencies',
    execute = function()
        local cacheDir = 'TempestCache'
        -- Check if there is a cache directory
        if not os.isdir(cacheDir) then
            os.mkdir(cacheDir)
        end

        if _TARGET_OS == 'windows' then
            local slangZipPath = path.join(cacheDir, 'slang-2024.14.6-win64.zip')
            local downloaded = fetchIfNotExists('https://github.com/shader-slang/slang/releases/download/v2024.14.6/slang-2024.14.6-windows-x86_64.zip', slangZipPath)
            if downloaded then
                zip.extract(slangZipPath, path.join(_MAIN_SCRIPT_DIR, 'dependencies/slang'))
            end
        end
    end
}

local slangPath = (function ()
    if _TARGET_OS == 'windows' then
        return path.join(_MAIN_SCRIPT_DIR, 'dependencies/slang/bin/slangc.exe')
    end
end)()

local fetch = {
    slang = slangPath
}

return fetch