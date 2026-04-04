local function ninja_test_target(wks)
    -- For each test project, create a target to run the test (for each configuration-platform pair)
    -- Create a meta-target to run all tests in a configuration-platform pair
    
    -- Write rule to run a test target
    _p('')
    _p('rule run_test')
    if os.shell() == 'posix' then
        _p('  command = sh -c \'$in --gtest_brief=1\'')
    else
        _p('  command = cmd /c "$in --gtest_brief=1"')
    end

    _p('')

    -- Build table of configuration-platform pairs to test targets
    local test_targets = {}
    
    for cfg in premake.workspace.eachconfig(wks) do
        local configkey = nil
        if cfg.platform then
            configkey = cfg.buildcfg .. '_' .. cfg.platform
        else
            configkey = cfg.buildcfg
        end

        local targets = {}
        for prj in premake.workspace.eachproject(wks) do
            -- Split group name by '/' and check if 'Tests' is the last element
            local group_parts = {}
            if prj.group then
                for part in string.gmatch(prj.group, '([^/]+)') do
                    table.insert(group_parts, part)
                end
            end

            local is_test_project = #group_parts > 0 and group_parts[#group_parts] == 'Tests'

            if is_test_project then
                local prj_cfg = premake.project.getconfig(prj, cfg.buildcfg, cfg.platform)
                if prj_cfg then
                    local prj_key = premake.modules.ninja.key(prj_cfg)
                    table.insert(targets, {
                        key = prj_key,
                        path = path.getrelative(cfg.workspace.location, prj_cfg.buildtarget.directory) .. "/" .. prj_cfg.buildtarget.name
                    })
                end
            end
        end

        local existing_targets = test_targets[configkey]
        if existing_targets then
            existing_targets = table.join(existing_targets, targets)
        else
            existing_targets = targets
        end
        test_targets[configkey] = existing_targets
    end

    local sorted_configkeys = table.keys(test_targets)
    table.sort(sorted_configkeys)

    -- Emit individual test targets
    for _, configkey in ipairs(sorted_configkeys) do
        local targets = test_targets[configkey]
        if #targets == 0 then
            goto continue
        end

        for _, target in ipairs(targets) do
            local test_target_name = 'test_' .. configkey .. '_' .. target.key

            _p('build %s: run_test %s', test_target_name, target.path)
        end

        -- Emit meta-target for this configuration-platform pair
        local meta_target_name = 'test_' .. configkey
        _p('build %s: phony %s', meta_target_name, table.concat(table.translate(targets, function(target)
            return 'test_' .. configkey .. '_' .. target.key
        end), ' '))

        ::continue::
    end
end

if _ACTION == 'ninja' then

    require('ninja')

    premake.override(premake.modules.ninja.wks, "elements", function(base, wks)
        local calls = base(wks)
        table.insertafter(calls, premake.modules.ninja.wks.phonyConfigPlatformPairs, ninja_test_target)
        return calls
    end)

end