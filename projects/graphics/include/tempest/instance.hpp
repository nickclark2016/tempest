#ifndef tempest_instance_hpp__
#define tempest_instance_hpp__

#include <cstdint>
#include <memory>
#include <string_view>

namespace tempest::graphics
{
    class iinstance;

    class idevice
    {
      public:
        virtual ~idevice() = default;
    };

    class iinstance
    {
      public:
        virtual ~iinstance() = default;
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
