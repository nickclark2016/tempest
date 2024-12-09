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
            'X11',
        }
    end
    
    function m.applyTempestConfig()
        dependson {
            'tempest',
        }
        
        links {
            'tempest',
        }
        
        m.applyTempestInternalConfig()
    end

    return m