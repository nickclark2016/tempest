#include <tempest/algorithm.hpp>
#include <tempest/charconv.hpp>
#include <tempest/files.hpp>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace tempest
{
    namespace
    {
#ifdef _WIN32
        auto map_win32_error(DWORD err) -> file_error
        {
            switch (err)
            {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                return file_error::not_found;
            case ERROR_ACCESS_DENIED:
                return file_error::access_denied;
            case ERROR_FILE_EXISTS:
            case ERROR_ALREADY_EXISTS:
                return file_error::already_exists;
            case ERROR_INVALID_PARAMETER:
            case ERROR_INVALID_NAME:
                return file_error::invalid_argument;
            default:
                return file_error::io_failure;
            }
        }
#else
        auto map_posix_error(int err) -> file_error
        {
            switch (err)
            {
            case ENOENT:
                return file_error::not_found;
            case EACCES:
            case EPERM:
                return file_error::access_denied;
            case EEXIST:
                return file_error::already_exists;
            case EINVAL:
            case EISDIR:
                return file_error::invalid_argument;
            default:
                return file_error::io_failure;
            }
        }
#endif
    } // namespace

#ifdef _WIN32
    auto read_file_to_vector(const filesystem::path& path) -> expected<vector<byte>, file_error>
    {
        const auto attrs = GetFileAttributesW(path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0U)
        {
            return unexpected(file_error::invalid_argument);
        }

        auto* handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);

        if (handle == INVALID_HANDLE_VALUE)
        {
            return unexpected(map_win32_error(GetLastError()));
        }

        LARGE_INTEGER size_val;
        if (GetFileSizeEx(handle, &size_val) == 0)
        {
            auto err = map_win32_error(GetLastError());
            CloseHandle(handle);
            return unexpected(err);
        }

        auto file_size = static_cast<size_t>(size_val.QuadPart);
        auto result = vector<byte>{};
        if (file_size == 0)
        {
            CloseHandle(handle);
            return result;
        }

        unsafe::resize_no_init(result, file_size);

        auto total_read = size_t{0};
        while (total_read < file_size)
        {
            auto bytes_to_read = static_cast<DWORD>(min(file_size - total_read, static_cast<size_t>(MAXDWORD)));
            DWORD bytes_read = 0;
            if ((ReadFile(handle, result.data() + total_read, bytes_to_read, &bytes_read, nullptr) == 0) ||
                bytes_read == 0)
            {
                auto err = map_win32_error(GetLastError());
                CloseHandle(handle);
                return unexpected(err);
            }
            total_read += bytes_read;
        }

        CloseHandle(handle);
        return result;
    }

    auto read_file_to_string(const filesystem::path& path) -> expected<string, file_error>
    {
        const auto attrs = GetFileAttributesW(path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0U)
        {
            return unexpected(file_error::invalid_argument);
        }

        auto* handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);

        if (handle == INVALID_HANDLE_VALUE)
        {
            return unexpected(map_win32_error(GetLastError()));
        }

        LARGE_INTEGER size_val;
        if (GetFileSizeEx(handle, &size_val) == 0)
        {
            auto err = map_win32_error(GetLastError());
            CloseHandle(handle);
            return unexpected(err);
        }

        auto file_size = static_cast<size_t>(size_val.QuadPart);
        auto result = string{};
        if (file_size == 0)
        {
            CloseHandle(handle);
            return result;
        }

        result.resize(file_size);

        auto total_read = size_t{0};
        while (total_read < file_size)
        {
            auto bytes_to_read = static_cast<DWORD>(min(file_size - total_read, static_cast<size_t>(MAXDWORD)));
            DWORD bytes_read = 0;
            if ((ReadFile(handle, result.data() + total_read, bytes_to_read, &bytes_read, nullptr) == 0) ||
                bytes_read == 0)
            {
                auto err = map_win32_error(GetLastError());
                CloseHandle(handle);
                return unexpected(err);
            }
            total_read += bytes_read;
        }

        CloseHandle(handle);
        return result;
    }

    auto write_file_from_bytes(const filesystem::path& path, span<const byte> data) -> expected<void, file_error>
    {
        auto* handle =
            CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (handle == INVALID_HANDLE_VALUE)
        {
            return unexpected(map_win32_error(GetLastError()));
        }

        auto total_written = size_t{0};
        auto total_size = data.size();
        while (total_written < total_size)
        {
            auto bytes_to_write = static_cast<DWORD>(min(total_size - total_written, static_cast<size_t>(MAXDWORD)));
            DWORD bytes_written = 0;
            if ((WriteFile(handle, data.data() + total_written, bytes_to_write, &bytes_written, nullptr) == 0) ||
                bytes_written == 0)
            {
                auto err = map_win32_error(GetLastError());
                CloseHandle(handle);
                return unexpected(err);
            }
            total_written += bytes_written;
        }

        CloseHandle(handle);
        return {};
    }
#else
    auto read_file_to_vector(const filesystem::path& path) -> expected<vector<byte>, file_error>
    {
        auto fd = open(path.c_str(), O_RDONLY);
        if (fd == -1)
        {
            return unexpected(map_posix_error(errno));
        }

        struct stat st;
        if (fstat(fd, &st) != 0)
        {
            auto err = map_posix_error(errno);
            close(fd);
            return unexpected(err);
        }

        if (S_ISDIR(st.st_mode))
        {
            close(fd);
            return unexpected(file_error::invalid_argument);
        }

        auto file_size = static_cast<size_t>(st.st_size);
        auto result = vector<byte>{};
        if (file_size == 0)
        {
            close(fd);
            return result;
        }

        unsafe::resize_no_init(result, file_size);

        auto total_read = size_t{0};
        while (total_read < file_size)
        {
            auto res = read(fd, reinterpret_cast<char*>(result.data()) + total_read, file_size - total_read);
            if (res < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                auto err = map_posix_error(errno);
                close(fd);
                return unexpected(err);
            }
            if (res == 0)
            {
                close(fd);
                return unexpected(file_error::io_failure);
            }
            total_read += static_cast<size_t>(res);
        }

        close(fd);
        return result;
    }

    auto read_file_to_string(const filesystem::path& path) -> expected<string, file_error>
    {
        auto fd = open(path.c_str(), O_RDONLY);
        if (fd == -1)
        {
            return unexpected(map_posix_error(errno));
        }

        struct stat st;
        if (fstat(fd, &st) != 0)
        {
            auto err = map_posix_error(errno);
            close(fd);
            return unexpected(err);
        }

        if (S_ISDIR(st.st_mode))
        {
            close(fd);
            return unexpected(file_error::invalid_argument);
        }

        auto file_size = static_cast<size_t>(st.st_size);
        auto result = string{};
        if (file_size == 0)
        {
            close(fd);
            return result;
        }

        result.resize(file_size);

        auto total_read = size_t{0};
        while (total_read < file_size)
        {
            auto res = read(fd, result.data() + total_read, file_size - total_read);
            if (res < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                auto err = map_posix_error(errno);
                close(fd);
                return unexpected(err);
            }
            if (res == 0)
            {
                close(fd);
                return unexpected(file_error::io_failure);
            }
            total_read += static_cast<size_t>(res);
        }

        close(fd);
        return result;
    }

    auto write_file_from_bytes(const filesystem::path& path, span<const byte> data) -> expected<void, file_error>
    {
        auto fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1)
        {
            return unexpected(map_posix_error(errno));
        }

        auto total_written = size_t{0};
        auto total_size = data.size();
        while (total_written < total_size)
        {
            auto res =
                write(fd, reinterpret_cast<const char*>(data.data()) + total_written, total_size - total_written);
            if (res < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                auto err = map_posix_error(errno);
                close(fd);
                return unexpected(err);
            }
            if (res == 0)
            {
                close(fd);
                return unexpected(file_error::io_failure);
            }
            total_written += static_cast<size_t>(res);
        }

        close(fd);
        return {};
    }
#endif
} // namespace tempest