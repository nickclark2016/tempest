#ifndef tempest_camera_slang
#define tempest_camera_slang

struct Camera {
    float4x4 projection;
    float4x4 inv_projection;
    float4x4 view;
    float4x4 inv_view;
    float4 eye;
};

#endif // tempest_camera_slang