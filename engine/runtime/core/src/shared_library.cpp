#include <tempest/shared_library.hpp>

#if defined(TEMPEST_PLATFORM_WINDOWS)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(TEMPEST_PLATFORM_LINUX) || defined(TEMPEST_PLATFORM_MACOS)
#include <dlfcn.h>
#else
#error "Unsupported platform for shared library loading"
#endif

namespace tempest
{
#ifdef TEMPEST_PLATFORM_WINDOWS
    struct shared_library::_impl
    {
        HMODULE handle;
    };

    namespace
    {
        auto load_library(const filesystem::path& lib_path) -> expected<HMODULE, shared_library::load_error>
        {
            if (!filesystem::exists(lib_path))
            {
                return unexpected(shared_library::load_error::file_not_found);
            }

            const auto existing_error = GetLastError();
            SetLastError(ERROR_SUCCESS);

            auto* const handle = LoadLibraryW(lib_path.c_str());
            if (handle == nullptr)
            {
                const auto load_error = GetLastError();
                const auto error_code = [&]() -> shared_library::load_error {
                    switch (load_error) {
                        case ERROR_FILE_NOT_FOUND:
                            return shared_library::load_error::file_not_found;
                        case ERROR_PATH_NOT_FOUND:
                            return shared_library::load_error::path_not_found;
                        case ERROR_INVALID_HANDLE:
                            [[fallthrough]];
                        case ERROR_INVALID_DLL:
                            return shared_library::load_error::invalid_shared_library;
                        case ERROR_INVALID_ACCESS:
                            return shared_library::load_error::invalid_permissions;
                        case ERROR_MOD_NOT_FOUND:
                            [[fallthrough]];
                        case ERROR_DLL_NOT_FOUND:
                            return shared_library::load_error::missing_dependency;
                        default:
                            return shared_library::load_error::unknown_error;
                    }
                }();

                SetLastError(existing_error);
                return unexpected(error_code);
            }

            SetLastError(existing_error);

            return handle;
        }
    } // namespace
#elif defined(TEMPEST_PLATFORM_LINUX) || defined(TEMPEST_PLATFORM_MACOS)
    struct shared_library::_impl
    {
        void* handle;
    };

    namespace
    {
        auto load_library(const filesystem::path& lib_path) -> expected<void*, shared_library::load_error>
        {
            if (!filesystem::exists(lib_path))
            {
                return unexpected(shared_library::load_error::file_not_found);
            }

            dlerror(); // Clear any existing error

            auto* const handle = dlopen(lib_path.c_str(), RTLD_NOW);
            if (handle == nullptr)
            {
                const auto* error_msg = dlerror();
                shared_library::load_error error_code = shared_library::load_error::unknown_error;

                if (error_msg != nullptr)
                {
                    auto error_view = string_view(error_msg);
                    if (search(error_view, "No such file") != error_view.end())
                    {
                        error_code = shared_library::load_error::file_not_found;
                    }
                    else if (search(error_view, "Permission denied") != error_view.end())
                    {
                        error_code = shared_library::load_error::invalid_permissions;
                    }
                    else if (search(error_view, "invalid ELF header") != error_view.end() ||
                             search(error_view, "mach-o") != error_view.end())
                    {
                        error_code = shared_library::load_error::invalid_shared_library;
                    }
                    else if (search(error_view, "undefined symbol") != error_view.end() ||
                             search(error_view, "symbol not found") != error_view.end())
                    {
                        error_code = shared_library::load_error::missing_dependency;
                    }
                }

                return unexpected(error_code);
            }

            return handle;
        }
    }
#endif
    shared_library::~shared_library()
    {
        if (_pimpl)
        {
#ifdef TEMPEST_PLATFORM_WINDOWS
            FreeLibrary(_pimpl->handle);
#elif defined(TEMPEST_PLATFORM_LINUX) || defined(TEMPEST_PLATFORM_MACOS)
            dlclose(_pimpl->handle);
#endif
        }
    }

    expected<shared_library, shared_library::load_error> shared_library::load(const filesystem::path& library_path) noexcept
    {
        auto result = load_library(library_path);
        if (!result)
        {
            return unexpected(result.error());
        }

        shared_library lib;
        lib._pimpl = make_unique<_impl>();
        lib._pimpl->handle = *result;
        return lib;
    }
} // namespace tempest