workspace 'Tempest'
    configurations { 'Debug', 'Release' }
    platforms { 'x64' }

    filter 'configurations:Debug'
        defines { '_DEBUG' }
        symbols 'On'

    filter 'configurations:Release'
        defines { 'NDEBUG' }
        optimize 'On'

    filter 'platforms:x64'
        architecture 'x86_64'

    filter 'system:windows'
        systemversion 'latest'
        staticruntime 'Off'

    filter {}

    root = path.getdirectory(_MAIN_SCRIPT)

    binaries = '%{root}/bin/%{cfg.buildcfg}/%{cfg.system}/%{cfg.architecture}'
    intermidates = '%{root}/bin-int/%{cfg.buildcfg}/%{cfg.system}/%{cfg.architecture}'

    filter { 'action:vs*' }
        flags {
            'MultiProcessorCompile',
        }

    filter {}

    IncludeDir = {}

    startproject 'editor'

    include 'dependencies'
    include 'projects'

workspace ''

-- Fetch binary dependencies

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
            local slangZipPath = path.join(cacheDir, 'slang-2024.1.6-win64.zip')
            local downloaded = fetchIfNotExists('https://github.com/shader-slang/slang/releases/download/v2024.1.6/slang-2024.1.6-win64.zip', slangZipPath)
            if downloaded then
                zip.extract(slangZipPath, path.join(_MAIN_SCRIPT_DIR, 'dependencies/slang'))
            end
        end
    end
}