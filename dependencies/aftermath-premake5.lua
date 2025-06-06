scoped.project('aftermath', function()
    kind 'Utility'
    language 'C++'
    cppdialect 'C++20'

    location '%{root}/dependencies/aftermath'

    scoped.usage('INTERFACE', function()
        externalincludedirs('%{root}/dependencies/aftermath/include')
        libdirs('%{root}/dependencies/aftermath/lib/x64')
        links('GFSDK_Aftermath_Lib.x64')
    end)

    scoped.usage('aftermath-runtime', function()
        postbuildcommands {
            '{COPYFILE} %{root}/dependencies/aftermath/lib/x64/GFSDK_Aftermath_Lib.x64.dll %{cfg.targetdir}',
        }
    end)
end)