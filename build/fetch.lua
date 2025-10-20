local fetch = {}

local function fetch_if_not_exists(url, filename)
    if not os.isfile(filename) then
        print('Saving ' .. url .. ' to ' .. filename)
        local res, response = http.download(url, filename)
        if response ~= 200 then
            print('Response: ' .. tostring(response) .. ' for ' .. url)
            os.remove(filename) -- Remove the file if the download failed
        end
        return response == 200
    end
    return false
end

local function get_cache_dir()
    local cache_dir = 'TempestCache'

    if not os.isdir(cache_dir) then
        os.mkdir(cache_dir)
    end

    return cache_dir
end

local slang_dep_path = path.join(_MAIN_SCRIPT_DIR, 'dependencies/slang')

local function fetch_slang(cache_dir)
    if os.target() == 'windows' then
        local slang_zip_path = path.join(cache_dir, 'slang-2025.19.1-win64.zip')
        local downloaded = fetch_if_not_exists('https://github.com/shader-slang/slang/releases/download/v2025.19.1/slang-2025.19.1-windows-x86_64.zip', slang_zip_path)

        if downloaded then
            zip.extract(slang_zip_path, slang_dep_path)
            
            local files = os.matchfiles(path.join(slang_dep_path, '**'))
            for _, file in ipairs(files) do
                -- Ensure the files have read and write permissions. If the file is an executable, set it to be executable as well.
                if os.isfile(file) then
                    if os.isfile(file) and (file:endswith('.exe') or file:endswith('.dll')) then
                        os.chmod(file, '755') -- Set executable permissions for .exe and .dll files
                    else
                        os.chmod(file, '644') -- Set read/write permissions for other files
                    end
                end
            end
        end
    end

    if os.target() == 'linux' then
        local slang_zip_path = path.join(cache_dir, 'slang-2025.19.1-linux-64.zip')
        local downloaded = fetch_if_not_exists('https://github.com/shader-slang/slang/releases/download/v2025.19.1/slang-2025.19.1-linux-x86_64.zip', slang_zip_path)

        if downloaded then
            zip.extract(slang_zip_path, slang_dep_path)

            local files = os.matchfiles(path.join(slang_dep_path, '**'))
            for _, file in ipairs(files) do
                -- Ensure the files have read and write permissions. If the file is in the bin directory, set it to be executable as well.
                if os.isfile(file) then
                    if file:find('bin/') then
                        os.chmod(file, '755') -- Set executable permissions for files in the bin directory
                    else
                        os.chmod(file, '644') -- Set read/write permissions for other files
                    end
                end
            end
        end
    end
end

local aftermath_dep_path = path.join(_MAIN_SCRIPT_DIR, 'dependencies/aftermath')

local function fetch_aftermath(cache_dir)
    if not _OPTIONS['enable-aftermath'] then
        return
    end

    if os.target() == 'windows' then
        local aftermath_zip_path = path.join(cache_dir, 'NVIDIA_Nsight_Aftermath_SDK_2025.1.0.25009.zip')
        local downloaded = fetch_if_not_exists('https://developer.nvidia.com/downloads/assets/tools/secure/nsight-aftermath-sdk/2025_1_0/windows/NVIDIA_Nsight_Aftermath_SDK_2025.1.0.25009.zip', aftermath_zip_path)

        if downloaded then
            zip.extract(aftermath_zip_path, aftermath_dep_path)

            local files = os.matchfiles(path.join(aftermath_dep_path, '**'))
            for _, file in ipairs(files) do
                -- Ensure the files have read and write permissions. If the file is an executable, set it to be executable as well.
                if os.isfile(file) then
                    if os.isfile(file) and (file:endswith('.exe') or file:endswith('.dll')) then
                        os.chmod(file, '755') -- Set executable permissions for .exe and .dll files
                    else
                        os.chmod(file, '644') -- Set read/write permissions for other files
                    end
                end
            end
        end
    else
        p.error('Unsupported target for aftermath: ' .. os.target())
    end
end

fetch.slang = {
    directory = path.join(_MAIN_SCRIPT_DIR, 'dependencies/slang'),
    compiler = (function()
        if os.target() == 'windows' then
            return path.join(slang_dep_path, 'bin/slangc.exe')
        elseif os.target() == 'linux' then
            return path.join(slang_dep_path, 'bin/slangc')
        else
            print('Unsupported target: ' .. os.target())
        end
        return nil -- Return nil if the target is not supported
    end)()
}

fetch.aftermath = {
    directory = path.join(_MAIN_SCRIPT_DIR, 'dependencies/aftermath'),
}

local function fetch_poly_haven(cache_dir)
    local poly_haven_path = path.join(_MAIN_SCRIPT_DIR, 'vendor/polyhaven')
    
    if not os.isdir(poly_haven_path) then
        os.mkdir(poly_haven_path)
    end

    -- Ensure this directory has a .gitignore file to avoid committing binary files
    local gitignore_path = path.join(poly_haven_path, '.gitignore')
    if not os.isfile(gitignore_path) then
        local gitignore_content = [[
# Ignore all files
*
]]
        io.writefile(gitignore_path, gitignore_content)
    end

    local function fetch_hdri(hdri_name)
        -- Ensure the HDRI folder exists
        local hdri_path = path.join(poly_haven_path, 'hdri')
        if not os.isdir(hdri_path) then
            os.mkdir(hdri_path)
        end

        hdri_name = hdri_name:lower():gsub(' ', '_')

        local url = 'https://dl.polyhaven.org/file/ph-assets/HDRIs/exr/4k/' .. hdri_name .. '_4k.exr'
        local path = path.join(hdri_path, hdri_name .. '.exr')
        fetch_if_not_exists(url, path)
    end

    -- Iterate over the list of HDRIs to fetch
    for _, hdri_name in ipairs(fetch.polyhaven.hdri) do
        fetch_hdri(hdri_name)
    end
end

fetch.polyhaven = {
    directory = path.join(_MAIN_SCRIPT_DIR, 'vendor/polyhaven'),
    hdri = {
        'autumn_field_puresky'
    }
}

newaction {
    trigger = 'fetch',
    description = 'Fetch binary dependencies',
    execute = function()
        local cache_dir = get_cache_dir()
        fetch_slang(cache_dir)
        fetch_aftermath(cache_dir)
        fetch_poly_haven(cache_dir)
    end
}

newaction {
    trigger = 'fetch-clean',
    description = 'Clean up fetched files',
    execute = function()
        local cache_dir = get_cache_dir()
        if os.isdir(cache_dir) then
            os.rmdir(cache_dir)
        end

        -- For each table in fetch, check if it has a directory and remove it
        for _, dep in pairs(fetch) do
            print('Cleaning up ' .. dep.directory)
            if dep.directory and os.isdir(dep.directory) then
                print('Removing ' .. dep.directory)
                local res, msg = os.rmdir(dep.directory)
                if not res then
                    print('Failed to remove ' .. dep.directory .. ': ' .. msg)
                end
            end
        end
    end
}

return fetch
