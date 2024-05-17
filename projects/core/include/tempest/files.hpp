#ifndef tempest_core_files_hpp
#define tempest_core_files_hpp

#include <tempest/vector.hpp>

#include <string>
#include <string_view>

namespace tempest::core
{
    vector<std::byte> read_bytes(std::string_view path);
    std::string read_text(std::string_view path);
}

#endif // tempest_core_files_hpp