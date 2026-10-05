#include <tempest/server/server_context.hpp>

#include <tempest/optional.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace
{
    auto substr(tempest::string_view sv, size_t offset) noexcept -> tempest::string_view
    {
        if (offset >= sv.size())
        {
            return {};
        }
        return tempest::string_view{sv.data() + offset, sv.size() - offset};
    }

    auto parse_u32(tempest::string_view str) noexcept -> tempest::optional<tempest::uint32_t>
    {
        if (str.empty())
        {
            return tempest::nullopt;
        }
        auto val = 0U;
        for (const auto c : str)
        {
            if (c < '0' || c > '9')
            {
                return tempest::nullopt;
            }
            val = val * 10 + static_cast<tempest::uint32_t>(c - '0');
        }
        return val;
    }

    auto parse_float(tempest::string_view str) noexcept -> tempest::optional<float>
    {
        if (str.empty())
        {
            return tempest::nullopt;
        }
        auto val = 0.0F;
        auto frac = 0.0F;
        auto frac_scale = 1.0F;
        auto in_frac = false;
        for (const auto c : str)
        {
            if (c == '.')
            {
                if (in_frac)
                {
                    return tempest::nullopt;
                }
                in_frac = true;
                continue;
            }
            if (c < '0' || c > '9')
            {
                return tempest::nullopt;
            }
            if (!in_frac)
            {
                val = val * 10.0F + static_cast<float>(c - '0');
            }
            else
            {
                frac_scale *= 0.1F;
                frac += static_cast<float>(c - '0') * frac_scale;
            }
        }
        return val + frac;
    }
} // namespace

auto main(int argc, char* argv[]) -> int
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    auto config = tempest::server::server_config();

    for (auto i = 1; i < argc; ++i)
    {
        const auto arg = tempest::string_view(static_cast<const char*>(argv[i]));
        if (arg == "--test-run")
        {
            config.test_run = true;
        }
        else if (starts_with(arg, "--timeout="))
        {
            const auto val_str = substr(arg, 10);
            if (const auto parsed = parse_float(val_str))
            {
                config.test_timeout_seconds = *parsed;
            }
        }
        else if (starts_with(arg, "--port="))
        {
            const auto val_str = substr(arg, 7);
            if (const auto parsed = parse_u32(val_str))
            {
                config.port = static_cast<tempest::uint16_t>(*parsed);
            }
        }
        else if (starts_with(arg, "--fixed-timestep="))
        {
            const auto val_str = substr(arg, 17);
            if (const auto parsed = parse_float(val_str))
            {
                config.fixed_timestep = *parsed;
            }
        }
    }

    auto server = tempest::server::server_context(config);
    return server.run();
}
