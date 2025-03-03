project 'googletest'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++20'

    targetdir '%{binaries}'
    objdir '%{intermediates}'
    
    files {
        './src/gmock-all.cc',
        './src/gtest-all.cc',
    }

    includedirs {
        '.',
        './include',
    }

    usage "INTERFACE"
        externalincludedirs {
            '%{root}/dependencies/googletest/include',
        }

        dependson {
            'googletest',
        }

        links {
            'googletest',
        }
