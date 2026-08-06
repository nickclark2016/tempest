#include <tempest/vk/execution_port.hpp>

namespace tempest::rhi::vk
{
    execution_port::~execution_port() = default;

    auto execution_port::wait_idle() -> void
    {
    }

    auto execution_port::acquire_command_list(uint32_t thread_id, command_list_lifetime lifetime) -> rhi::command_list&
    {
        tempest::unreachable(); // Not Implemented
    }

    auto execution_port::submit(span<const rhi::command_list*> commands, span<const device_sync_point> wait_semaphores,
                                span<const device_sync_point> signal_semaphores) -> expected<void, submit_error>
    {
        tempest::unreachable(); // Not Implemented
    }
} // namespace tempest::rhi::vk