-- tempest.lua

    local m = {}

    function m.applyTempestInternalConfig()
        externalincludedirs {
            '%{IncludeDir.assets}',
            '%{IncludeDir.core}',
            '%{IncludeDir.ecs}',
            '%{IncludeDir.graphics}',
            '%{IncludeDir.logger}',
            '%{IncludeDir.math}',
        }

        includedirs {
            '%{IncludeDir.tempest}',
        }
    
        dependson {
            'assets',
            'core',
            'ecs',
            'graphics',
            'logger',
            'math',
        }
    
        links {
            -- Engine Projects
            'graphics',
            'assets',
            'ecs',
            'logger',
            'core',
            'math',
            -- Vendor Deps
            'glfw',
            'imgui',
            'simdjson',
            'spdlog',
            'tlsf',
            'vk-bootstrap',
            'vma',
        }

        scoped.filter({ 'system:linux' }, function()
            links { 'X11' }
        end)

        scoped.filter({
            'toolset:msc*'
        }, function()
            buildoptions {
                '/wd4324', -- 'structure was padded due to alignment specifier'
            }
        end)
    end
    
    function m.applyTempestConfig()
        dependson {
            'tempest',
        }
        
        links {
            'tempest',
        }
        
        m.applyTempestInternalConfig()

        removeincludedirs {
            '%{IncludeDir.tempest}',
        }

        externalincludedirs {
            '%{IncludeDir.tempest}',
        }

        externalwarnings 'Off'
        warnings 'Extra'
    end

    return m