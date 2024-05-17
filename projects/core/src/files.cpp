#include <tempest/files.hpp>

#include <cassert>
#include <fstream>
#include <sstream>

namespace tempest::core
{
    vector<std::byte> read_bytes(std::string_view path)
    {
        std::ifstream input(std::string(path), std::ios::ate | std::ios::binary);
        assert(input);
        size_t file_size = (size_t)input.tellg();
        vector<std::byte> buffer(file_size);
        input.seekg(0);
        input.read(reinterpret_cast<char*>(buffer.data()), file_size);
        return buffer;
    }

    std::string read_text(std::string_view path)
    {
        std::ostringstream buf;
        std::ifstream input(std::string(path), std::ios::ate | std::ios::binary);
        buf << input.rdbuf();
        return buf.str();
    }
} // namespace tempest::core