#include <tempest/instance.hpp>

#include "vk_instance.hpp"

namespace tempest::graphics
{
    std::unique_ptr<iinstance> instance_factory::create(const create_info& info)
    {
        return std::make_unique<vk::instance>(info);
    }
} // namespace tempest::graphics