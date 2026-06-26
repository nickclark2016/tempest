scoped.project('aftermath', function()
    kind 'Utility'
    language 'C++'
    cppdialect 'C++20'

    location 'aftermath'

    scoped.usage('INTERFACE', function()
        externalincludedirs('aftermath/include')
        libdirs('aftermath/lib/x64')
        links('GFSDK_Aftermath_Lib.x64')
    end)

    scoped.usage('aftermath-runtime', function()
        postbuildcommands {
            '{COPYFILE} aftermath/lib/x64/GFSDK_Aftermath_Lib.x64.dll %{cfg.targetdir}',
        }
    end)
end)