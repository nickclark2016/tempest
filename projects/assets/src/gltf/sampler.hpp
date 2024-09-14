#ifndef tempest_assets_gltf_sampler_hpp
#define tempest_assets_gltf_sampler_hpp

#include <tempest/int.hpp>
#include <tempest/string.hpp>

namespace tempest::assets::gltf
{
    enum class min_filter
    {
        NEAREST = 9728,
        LINEAR = 9729,
        NEAREST_MIPMAP_NEAREST = 9984,
        LINEAR_MIPMAP_NEAREST = 9985,
        NEAREST_MIPMAP_LINEAR = 9986,
        LINEAR_MIPMAP_LINEAR = 9987
    };

    enum class mag_filter
    {
        NEAREST = 9728,
        LINEAR = 9729
    };

    enum class wrap_mode
    {
        CLAMP_TO_EDGE = 33071,
        MIRRORED_REPEAT = 33648,
        REPEAT = 10497
    };

    struct sampler
    {
        min_filter min = min_filter::NEAREST;
        mag_filter mag = mag_filter::NEAREST;
        wrap_mode wrap_s = wrap_mode::REPEAT;
        wrap_mode wrap_t = wrap_mode::REPEAT;
        string name;

        // String representation of JSON objects
        string extension;
        string extras;
    };
} // namespace tempest::assets::gltf

#endif // tempest_assets_gltf_sampler_hpp