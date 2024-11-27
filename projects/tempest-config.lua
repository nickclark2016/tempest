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
            -- Vendor Deps
            'glfw',
            'imgui',
            'simdjson',
            'spdlog',
            'tlsf',
            'vk-bootstrap',
            'vma',
            -- Engine Projects
            'assets',
            'core',
            'ecs',
            'graphics',
            'logger',
            'math',
        }
    end
    
    function m.applyTempestConfig()
        m.applyTempestInternalConfig()
    
        dependson {
            'tempest',
        }
    
        links {
            'tempest',
        }
    end

    return m