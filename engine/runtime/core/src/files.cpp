#include <tempest/files.hpp>

#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

namespace tempest::core
{
    vector<byte> read_bytes(string_view path)
    {
        std::ifstream input(std::string(path.data(), path.size()), std::ios::ate | std::ios::binary);
        if (!input.is_open())
        {
            return {};
        }
        size_t file_size = (size_t)input.tellg();
        vector<byte> buffer;
        unsafe::resize_no_init(buffer, file_size);
        input.seekg(0);
        input.read(reinterpret_cast<char*>(buffer.data()), file_size);
        return buffer;
    }

    string read_text(string_view path)
    {
        std::ostringstream buf;
        std::ifstream input(std::string(path.data(), path.size()), std::ios::ate | std::ios::binary);
        if (!input.is_open())
        {
            return {};
        }
        buf << input.rdbuf();
        return buf.str().c_str();
    }
} // namespace tempest::core