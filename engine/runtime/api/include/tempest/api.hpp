#ifndef TEMPEST_API_API_HPP
#define TEMPEST_API_API_HPP

#if defined(TEMPEST_SHARED_LIB)
#   if defined(_WIN32)
#      if defined(TEMPEST_API_EXPORT)
#          define TEMPEST_API __declspec(dllexport)
#      else
#          define TEMPEST_API __declspec(dllimport)
#      endif
#   elif defined(__clang__) || defined(__GNUC__)
#      if defined(TEMPEST_API_EXPORT)
#          define TEMPEST_API __attribute__((visibility("default")))
#      else
#          define TEMPEST_API
#      endif
#   else
#      error "Unsupported platform"
#   endif
#else
#   define TEMPEST_API
#endif

#endif // TEMPEST_API_API_HPP
