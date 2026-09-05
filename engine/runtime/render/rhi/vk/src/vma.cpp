#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_STATS_STRING_ENABLED 0
#define VMA_USE_STL_SHARED_MUTEX 0
#define VMA_CONFIGURATION_USER_INCLUDES_H "vma_config.hpp"

#include "vma_config.hpp"

#define VMA_IMPLEMENTATION

#ifdef _MSC_VER
#pragma warning(push, 4)
#pragma warning(disable : 4127)
#pragma warning(disable : 4100)
#pragma warning(disable : 4189)
#pragma warning(disable : 4324)
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#pragma clang diagnostic ignored "-Wunused-private-field"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif

#include <vk_mem_alloc.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if defined(_MUTEX_) || defined(_SHARED_MUTEX_) || defined(_FSTREAM_) || defined(_GLIBCXX_MUTEX) ||                    \
    defined(_GLIBCXX_SHARED_MUTEX) || defined(_GLIBCXX_FSTREAM)
#error "Zero-STL violation: STL concurrency/fstream headers were included in VMA translation unit!"
#endif
