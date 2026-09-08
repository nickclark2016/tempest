#include <tempest/print.hpp>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace tempest
{
#ifdef _WIN32
    namespace
    {
        void write_to_windows_handle(DWORD std_handle_id, string_view text)
        {
            if (text.empty())
            {
                return;
            }

            auto *handle = GetStdHandle(std_handle_id);
            if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
            {
                return;
            }

            DWORD mode = 0;
            if (GetConsoleMode(handle, &mode) != 0)
            {
                DWORD written = 0;
                WriteConsoleA(handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
            }
            else
            {
                DWORD written = 0;
                WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
            }
        }
    } // namespace

    auto write_stdout(string_view text) -> void
    {
        write_to_windows_handle(STD_OUTPUT_HANDLE, text);
    }

    auto write_stderr(string_view text) -> void
    {
        write_to_windows_handle(STD_ERROR_HANDLE, text);
    }
#else
    auto write_stdout(string_view text) -> void
    {
        if (text.empty())
        {
            return;
        }
        auto total_written = size_t{0};
        while (total_written < text.size())
        {
            auto res = write(1, text.data() + total_written, text.size() - total_written);
            if (res <= 0)
            {
                break;
            }
            total_written += static_cast<size_t>(res);
        }
    }

    auto write_stderr(string_view text) -> void
    {
        if (text.empty())
        {
            return;
        }
        auto total_written = size_t{0};
        while (total_written < text.size())
        {
            auto res = write(2, text.data() + total_written, text.size() - total_written);
            if (res <= 0)
            {
                break;
            }
            total_written += static_cast<size_t>(res);
        }
    }
#endif
} // namespace tempest
