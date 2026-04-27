#ifndef tempest_core_shared_library_hpp
#define tempest_core_shared_library_hpp

// Utilities for loading shared libraries and retrieving function pointers.

#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/filesystem.hpp>

namespace tempest
{
    class shared_library
    {
      public:
        enum class load_error : uint8_t
        {
            file_not_found,
            path_not_found,
            invalid_permissions,
            invalid_shared_library,
            missing_dependency,
            unknown_error
        };

        [[nodiscard]]
        TEMPEST_API static auto load(const filesystem::path& library_path) noexcept
            -> expected<shared_library, load_error>;

        shared_library(const shared_library&) = delete;
        shared_library(shared_library&&) noexcept = default;
        ~shared_library();

        shared_library& operator=(const shared_library&) = delete;
        shared_library& operator=(shared_library&&) noexcept = default;

      private:
        shared_library() = default;

        struct _impl;

        unique_ptr<_impl> _pimpl;
    };
} // namespace tempest

#endif // tempest_core_shared_library_hpp
