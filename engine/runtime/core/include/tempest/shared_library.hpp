#ifndef tempest_core_shared_library_hpp
#define tempest_core_shared_library_hpp

// Utilities for loading shared libraries and retrieving function pointers.

#include "tempest/int.hpp"
#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/functional.hpp>

namespace tempest
{
    class TEMPEST_API shared_library
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

        enum class function_error : uint8_t
        {
            function_not_found,
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

        template <typename R, typename... Args>
        [[nodiscard]] auto get_function_handle(string_view function_name) const noexcept -> expected<function_ref<R(Args...)>, function_error>
        {
            auto result = _get_raw_function_handle(function_name);
            if (!result)
            {
                return unexpected(result.error());
            }

            return function_ref<R(Args...)>(reinterpret_cast<R(*)(Args...)>(result.value()));
        }

      private:
        shared_library() = default;

        struct _impl;

        unique_ptr<_impl> _pimpl;

        auto _get_raw_function_handle(string_view function_name) const noexcept -> expected<void*, function_error>;
    };
} // namespace tempest

#endif // tempest_core_shared_library_hpp
