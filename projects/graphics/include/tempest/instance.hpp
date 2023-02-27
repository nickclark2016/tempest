#ifndef tempest_instance_hpp__
#define tempest_instance_hpp__

#include "window.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace tempest::graphics
{
    class iinstance;

    class idevice
    {
      public:
        virtual ~idevice() = default;
        virtual void release() = 0;
    };

    class iinstance
    {
      public:
        virtual ~iinstance() = default;
        virtual std::span<const std::unique_ptr<idevice>> get_devices() const noexcept = 0;
        virtual void release() = 0;
    };

    class instance_factory
    {
      public:
        struct create_info
        {
            std::string_view name;
            std::uint32_t version_major;
            std::uint32_t version_minor;
            std::uint32_t version_patch;
        };

        static std::unique_ptr<iinstance> create(const create_info& info);
    };
} // namespace tempest::graphics

#endif // tempest_instance_hpp__
