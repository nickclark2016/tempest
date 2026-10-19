#ifndef tempest_game_module_hpp
#define tempest_game_module_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>

#ifndef TEMPEST_GAME_API
#    if defined(_WIN32)
#        if defined(TEMPEST_GAME_EXPORT)
#            define TEMPEST_GAME_API __declspec(dllexport)
#        else
#            define TEMPEST_GAME_API
#        endif
#    elif defined(__clang__) || defined(__GNUC__)
#        if defined(TEMPEST_GAME_EXPORT)
#            define TEMPEST_GAME_API __attribute__((visibility("default")))
#        else
#            define TEMPEST_GAME_API
#        endif
#    else
#        define TEMPEST_GAME_API
#    endif
#endif

namespace tempest
{
    class engine_context;

    namespace ecs
    {
        class component_type_registry;
    } // namespace ecs

    enum class network_mode : uint8_t
    {
        standalone_offline = 0,
        embedded_server,
        connect_remote,
    };

    struct play_settings
    {
        static constexpr network_mode default_mode = network_mode::standalone_offline;
        static constexpr string_view default_remote_address = "127.0.0.1";
        static constexpr uint16_t default_port = 7777;
        static constexpr bool default_start_embedded_server = true;

        network_mode mode = default_mode;
        string_view remote_address = default_remote_address;
        uint16_t port = default_port;
        bool start_embedded_server = default_start_embedded_server;
    };
} // namespace tempest

extern "C"
{
    TEMPEST_GAME_API auto tempest_game_register_types(tempest::ecs::component_type_registry& types) -> void;
    TEMPEST_GAME_API auto tempest_game_on_load(tempest::engine_context& ctx, tempest::span<const tempest::string_view> args) -> void;
    TEMPEST_GAME_API auto tempest_game_on_unload() -> void;
    TEMPEST_GAME_API auto tempest_game_on_play_begin(const tempest::play_settings& settings) -> void;
    TEMPEST_GAME_API auto tempest_game_on_play_end() -> void;
}

#endif // tempest_game_module_hpp
