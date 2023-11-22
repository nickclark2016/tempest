#ifndef tempest_core_files_hpp
#define tempest_core_files_hpp

#include <string>
#include <string_view>
#include <vector>

namespace tempest::core
{
    std::vector<std::byte> read_bytes(std::string_view path);
    std::string read_text(std::string_view path);
}

#endif // tempest_core_files_hpp