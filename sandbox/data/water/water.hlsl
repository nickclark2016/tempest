#include "../common/camera.hlsl"
#include "../common/lighting.hlsl"

#define MESH_VERTEX_QUAD_X 1024
#define MESH_VERTEX_QUAD_Z 1024

[[vk::binding(0, 0)]] cbuffer Cameras {
    CameraData camera_data;
};
[[vk::binding(1, 0)]] cbuffer Lighting {
    DirectionalLight sun;
};
[[vk::binding(2, 0)]] cbuffer WaterSimState {
    float frequency;
    float frequency_multiplier;
    float initial_seed;
    float seed_iter;
    float amplitude;
    float amplitude_multiplier;
    float initial_speed;
    float speed_ramp;
    float drag;
    float height;
    float max_peak;
    float peak_offset;

    float time;
    int num_waves;
};

static const float k_pi = 3.14159265;
static const float k_fresnel_shininess = 10.0;
static const float k_fresnel_bias = 0.0;
static const float k_shininess = 0.8;
static const float k_fresnel_strength = 0.4;
static const float3 k_diffuse_reflectance = float3(0.1, 0.4, 0.6);
static const float3 k_specular_reflectance = float3(0.1, 0.3, 0.8);
static const float3 k_fresnel_color = float3(1, 1, 1);
static const float3 k_tip_color = float3(1, 1, 1);

// 1---2
// | / |
// 0---3

static const uint indices[6] = {
    0, 2, 3, 0, 1, 2
};

static const uint row_offset[6] = {
    0, 1, 1, 1, 0, 0,
};

static const uint col_offset[6] = {
    0, 1, 0, 1, 0, 1
};

struct PSInput
{
    float4 pixel_position : SV_POSITION;
    float4 world_position : POSITION;
};

float3 VertexFBM(float3 vertex_position)
{
    float f = frequency;
    float a = amplitude;
    float speed = initial_speed;
    float seed = initial_seed;

    float3 p = vertex_position;
    
    float amplitude_sum = 0.0f;
    
    float h = 0.0f;
    float2 n = 0.0f;

    for (int i = 0; i < num_waves; ++i)
    {
        float2 dir = normalize(float2(cos(seed), sin(seed)));
        float x = dot(dir, p.xz) * f + time * speed;
        float wave = a * exp(max_peak* sin(x) - peak_offset);
        float dx = max_peak * wave * cos(x);

        h += wave;

        p.xz += dir * -dx * a * drag;

        amplitude_sum += a;
        f *= frequency_multiplier;
        a *= amplitude_multiplier;
        speed *= speed_ramp;
        seed += seed_iter;
    }

    float3 output = float3(h, n.x, n.y) / amplitude_sum;
    output.x *= height;

    return output;
}

float3 FragmentFBM(float3 vertex_position)
{
    float f = frequency;
    float a = amplitude;
    float speed = initial_speed;
    float seed = initial_seed;

    float3 p = vertex_position;
    
    float amplitude_sum = 0.0f;
    
    float h = 0.0f;
    float2 n = 0.0f;

    for (int i = 0; i < num_waves; ++i)
    {
        float2 dir = normalize(float2(cos(seed), sin(seed)));
        float x = dot(dir, p.xz) * f + time * speed;
        float wave = a * exp(max_peak * sin(x) - peak_offset);
        float2 dw = f * dir * (max_peak * wave * cos(x));

        h += wave;
        p.xz += -dw * a * drag;

        n += dw;

        amplitude_sum += a;
        f *= frequency_multiplier;
        a *= amplitude_multiplier;
        speed *= speed_ramp;
        seed += seed_iter;
    }

    float3 output = float3(h, n.x, n.y) / amplitude_sum;
    output.x *= 1.34;

    return output;
}

PSInput VSMain(uint index_id : SV_VERTEXID)
{
    uint vert_id = index_id % 6;
    uint quad_id = index_id / 6;
    uint col = quad_id % MESH_VERTEX_QUAD_X;
    uint row = quad_id / MESH_VERTEX_QUAD_Z;

    uint x = col + col_offset[vert_id];
    uint z = row + row_offset[vert_id];

    x /= 2;
    z /= 2;

    float3 fbm = VertexFBM(float3(x, 0, z));
    float3 h = 0.0f;
    float3 n = 0.0f;

    h.y = fbm.x;
    n.xy = fbm.yz;

    float4 position = float4(x, 0, z, 1.0) + float4(h, 0.0);

    PSInput result;
    result.world_position = position;
    result.pixel_position = mul(camera_data.proj_matrix, mul(camera_data.view_matrix, position));

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 light_dir = -normalize(sun.direction.xyz);
    float3 view_dir = normalize(camera_data.position - input.world_position.xyz);
    float3 halfway_dir = normalize(light_dir + view_dir);

    float3 fbm = FragmentFBM(input.world_position.xyz);
    float height = fbm.x;
    float3 normal = float3(fbm.yz, 0);
    normal = normalize(float3(-normal.x, 1.0f, -normal.y));

    float ndot1 = clamp(dot(sun.direction.xyz, normal), 0, 1);
    float3 diffuse_refl = k_diffuse_reflectance / k_pi;
    float3 diffuse = sun.color_illum.rgb * ndot1 * diffuse_refl;

    // schlick fresnel
    float3 fres_norm = normal;
    float base = 1 - dot(view_dir, fres_norm);
    float exponential = pow(base, k_fresnel_shininess);
    float r = exponential + k_fresnel_bias * (1.0 - exponential);
    r *= k_fresnel_strength;
    
    float3 fresnel = r * k_fresnel_color;

    float3 spec_reflect = k_specular_reflectance;
    float3 spec_normal = normal;
    float spec = pow(clamp(dot(spec_normal, halfway_dir), 0, 1), k_shininess) * ndot1;
    float3 specular = sun.color_illum.rgb * spec_reflect * spec;

    base = 1 - clamp(dot(view_dir, halfway_dir), 0, 1);
    exponential = pow(base, 5.0);
    r = exponential + k_fresnel_bias * (1.0 - exponential);
    specular *= r;

    float3 tip_color = k_tip_color * pow(height, 10.0);
    float3 output = float3(0.0, 0.15, 0.45) + diffuse + specular + fresnel + tip_color;

    return float4(output, 1.0f);
}