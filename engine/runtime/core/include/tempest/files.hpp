#ifndef tempest_core_files_hpp
#define tempest_core_files_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::core
{
    TEMPEST_API vector<byte> read_bytes(string_view path);
    TEMPEST_API string read_text(string_view path);
} // namespace tempest::core

#endif // tempest_core_files_hpp