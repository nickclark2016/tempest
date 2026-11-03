#include <tempest/shared_library.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/string.hpp>

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
    namespace
    {
        static constexpr size_t stack_symbol_capacity = 256;

        template <typename Invocation>
        auto with_null_terminated_symbol(string_view function_name, Invocation&& invocation)
        {
            if (function_name.empty())
            {
                return invocation("");
            }

            if (function_name.size() < stack_symbol_capacity)
            {
                auto buffer = inplace_vector<char, stack_symbol_capacity>{};
                for (const auto character : function_name)
                {
                    buffer.push_back(character);
                }
                buffer.push_back('\0');
                return invocation(buffer.data());
            }

            const auto name_str = string{function_name};
            return invocation(name_str.c_str());
        }
    } // namespace

#ifdef TEMPEST_PLATFORM_WINDOWS
    struct shared_library::_impl
    {
        HMODULE handle;
    };

    namespace
    {
        auto load_library(const filesystem::path& lib_path) -> expected<HMODULE, shared_library::load_error>
        {
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

        auto load_function(HMODULE lib_handle, string_view function_name) -> expected<void*, shared_library::function_error>
        {
            return with_null_terminated_symbol(function_name, [&](const char* const symbol_name) -> expected<void*, shared_library::function_error> {
                const auto existing_error = GetLastError();
                SetLastError(ERROR_SUCCESS);

                auto* const func_handle = GetProcAddress(lib_handle, symbol_name);
                if (func_handle == nullptr)
                {
                    const auto load_error = GetLastError();
                    const auto error_code = load_error == ERROR_PROC_NOT_FOUND
                                                ? shared_library::function_error::function_not_found
                                                : shared_library::function_error::unknown_error;

                    SetLastError(existing_error);
                    return unexpected(error_code);
                }

                SetLastError(existing_error);
                return reinterpret_cast<void*>(func_handle);
            });
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

        auto load_function(void* lib_handle, string_view function_name) -> expected<void*, shared_library::function_error>
        {
            return with_null_terminated_symbol(function_name, [&](const char* const symbol_name) -> expected<void*, shared_library::function_error> {
                dlerror(); // Clear any existing error

                auto* const func_handle = dlsym(lib_handle, symbol_name);
                if (func_handle == nullptr)
                {
                    const auto* const error_msg = dlerror();
                    auto error_code = shared_library::function_error::unknown_error;

                    if (error_msg != nullptr)
                    {
                        const auto error_view = string_view{error_msg};
                        if (search(error_view, "undefined symbol") != error_view.end() ||
                            search(error_view, "symbol not found") != error_view.end())
                        {
                            error_code = shared_library::function_error::function_not_found;
                        }
                    }

                    return unexpected(error_code);
                }

                return func_handle;
            });
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
    
    expected<void*, shared_library::function_error> shared_library::_get_raw_function_handle(string_view function_name) const noexcept
    {
        return load_function(_pimpl->handle, function_name);
    }

    expected<shared_library, shared_library::load_error> shared_library::load(const filesystem::path& library_path) noexcept
    {
        auto result = load_library(library_path);
        if (!result)
        {
            return unexpected(result.error());
        }

        auto lib = shared_library{};
        lib._pimpl = make_unique<_impl>();
        lib._pimpl->handle = *result;
        return lib;
    }
} // namespace tempest