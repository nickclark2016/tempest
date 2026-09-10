#ifndef tempest_core_files_hpp
#define tempest_core_files_hpp

#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/int.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest
{
    /// @brief Represents categorized file I/O error conditions.
    enum class file_error : uint8_t
    {
        not_found,
        access_denied,
        already_exists,
        invalid_argument,
        io_failure,
        unknown,
    };

    /// @brief Reads an entire file into a byte vector buffer.
    /// @param path Path to the file to read.
    /// @return Expected containing the loaded bytes on success, or a file_error on failure.
    [[nodiscard]] TEMPEST_API auto read_file_to_vector(const filesystem::path& path) -> expected<vector<byte>, file_error>;

    /// @brief Reads an entire file into a string buffer.
    /// @param path Path to the file to read.
    /// @return Expected containing the loaded file text on success, or a file_error on failure.
    [[nodiscard]] TEMPEST_API auto read_file_to_string(const filesystem::path& path) -> expected<string, file_error>;

    /// @brief Writes a byte span to a file, creating or truncating it.
    /// @param path Path to the file to write.
    /// @param data Byte span to write.
    /// @return Expected void on success, or a file_error on failure.
    TEMPEST_API auto write_file_from_bytes(const filesystem::path& path, span<const byte> data)
        -> expected<void, file_error>;
} // namespace tempest

#endif // tempest_core_files_hpp