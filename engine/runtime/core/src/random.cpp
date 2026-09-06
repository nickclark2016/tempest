#include <tempest/random.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#if defined(_MSC_VER) || defined(__clang__)
#pragma comment(lib, "bcrypt.lib")
#endif

#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace tempest
{
    auto random_device::generate(void* buffer, size_t size) -> void
    {
#ifdef _WIN32
        BCryptGenRandom(nullptr, static_cast<PUCHAR>(buffer), static_cast<ULONG>(size),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
#else
        auto fd = open("/dev/urandom", O_RDONLY);
        if (fd >= 0)
        {
            auto total_read = static_cast<size_t>(0);
            auto* dest = static_cast<char*>(buffer);
            while (total_read < size)
            {
                const auto bytes_read = read(fd, dest + total_read, size - total_read);
                if (bytes_read <= 0)
                {
                    break;
                }
                total_read += static_cast<size_t>(bytes_read);
            }
            close(fd);
        }
#endif
    }

    auto random_device::operator()() -> result_type
    {
        auto value = static_cast<result_type>(0);
        generate(&value, sizeof(value));
        return value;
    }
} // namespace tempest