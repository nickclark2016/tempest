import "../common/camera";
import "../common/fsquad";
import "../common/material";
import "../common/scene";

struct SSAOPushConstants {
    uint64_t scene_constants_address;
    int32_t depth_texture_index;
    int32_t linear_sampler_index;
    int32_t point_sampler_index;
    float radius;
    float bias;
    float power;
    float _pad;
};

[[vk::push_constant]]
ConstantBuffer<SSAOPushConstants> push_constants;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

struct FragmentOutput {
    float visibility : SV_Target0;
};

[shader("vertex")]
VSOutput VSMain(uint id : SV_VertexID) {
    let vtx = generate_quad_vertex(id);
    return VSOutput(
        vtx.position,
        vtx.uv
    );
}

float interleaved_gradient_noise(float2 screen_pos) {
    float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
    return frac(magic.z * frac(dot(screen_pos, magic.xy)));
}

float3 calculate_view_position(float2 coords, float4x4 inv_proj, Texture2D depth_tex, SamplerState linear_sampler) {
    float frag_depth = -depth_tex.Sample(linear_sampler, coords).r;
    float2 xy = coords * 2.0 - 1.0;
    float4 ndc = float4(xy.x, xy.y, frag_depth, 1.0);
    float4 view_pos = mul(inv_proj, ndc);
    return view_pos.xyz / max(abs(view_pos.w), 1e-6);
}

[shader("fragment")]
FragmentOutput FSMain(VSOutput vs_out) {
    FragmentOutput output;
    if (push_constants.depth_texture_index < 0) {
        output.visibility = 1.0;
        return output;
    }

    SceneGlobals* scene = (SceneGlobals*)push_constants.scene_constants_address;
    float4x4 inv_proj = scene.camera.inv_projection;
    float4x4 proj = scene.camera.projection;

    SamplerState linear_sampler = bindless_samplers[push_constants.linear_sampler_index];
    Texture2D depth_tex = bindless_textures[push_constants.depth_texture_index];

    float raw_depth = depth_tex.Sample(linear_sampler, vs_out.uv).r;
    if (raw_depth <= 0.00001f) {
        output.visibility = 1.0;
        return output;
    }

    float3 view_position = calculate_view_position(vs_out.uv, inv_proj, depth_tex, linear_sampler);

    float3 d_pos_dx = ddx(view_position);
    float3 d_pos_dy = ddy(view_position);
    float3 view_normal = normalize(cross(d_pos_dx, d_pos_dy));
    if (length(view_normal) < 0.1f) {
        view_normal = float3(0.0, 0.0, -1.0);
    }

    float noise = interleaved_gradient_noise(vs_out.position.xy);
    float angle = noise * 6.2831853f;
    float sin_a = sin(angle);
    float cos_a = cos(angle);
    float3 random_dir = float3(cos_a, sin_a, 0.0);

    float3 tangent = normalize(random_dir - view_normal * dot(random_dir, view_normal));
    float3 bitangent = cross(view_normal, tangent);
    float3x3 tbn = transpose(float3x3(tangent, bitangent, view_normal));

    const float3 samples[16] = {
        float3( 0.5381, 0.1856, 0.4319),
        float3( 0.1379, 0.2486, 0.4430),
        float3( 0.3371, 0.5679, 0.0557),
        float3(-0.6999, -0.0451, 0.1019),
        float3( 0.0689, -0.1598, 0.8547),
        float3( 0.0560, 0.0069, 0.1843),
        float3(-0.0146, 0.1402, 0.0062),
        float3( 0.0100, -0.1924, 0.0344),
        float3(-0.3577, -0.5301, 0.4358),
        float3(-0.3169, 0.1063, 0.0158),
        float3( 0.0103, -0.5869, 0.0046),
        float3(-0.0897, -0.4940, 0.3287),
        float3( 0.7119, -0.0154, 0.0918),
        float3(-0.0533, 0.0596, 0.5411),
        float3( 0.0352, -0.0631, 0.5460),
        float3(-0.4776, 0.2847, 0.0271)
    };

    float occlusion = 0.0;
    float radius = push_constants.radius > 0.0 ? push_constants.radius : 0.5f;
    float bias = push_constants.bias > 0.0 ? push_constants.bias : 0.025f;

    for (int i = 0; i < 16; ++i) {
        float3 kernel_sample = samples[i];
        float3 sample_direction = mul(tbn, kernel_sample);
        float angle_norm = dot(sample_direction, view_normal);
        sample_direction *= (angle_norm >= 0.0 ? 1.0 : -1.0);
        
        float scale = float(i + 1) / 16.0f;
        scale = lerp(0.1f, 1.0f, scale * scale);
        // Offset sample position slightly along the normal to prevent flat surface self-occlusion
        float3 sample_position = view_position + (sample_direction + view_normal * 0.1f) * (radius * scale);

        float4 sample_clip_pos = mul(proj, float4(sample_position, 1.0));
        sample_clip_pos.xyz /= max(abs(sample_clip_pos.w), 1e-6);
        sample_clip_pos.xy = sample_clip_pos.xy * 0.5f + 0.5f;

        if (any(sample_clip_pos.xy < 0.0) || any(sample_clip_pos.xy > 1.0)) {
            continue;
        }

        float sample_depth = calculate_view_position(sample_clip_pos.xy, inv_proj, depth_tex, linear_sampler).z;
        float depth_delta = abs(view_position.z - sample_depth);

        if (sample_depth >= sample_position.z + bias && depth_delta < radius) {
            float range_factor = 1.0 - (depth_delta / radius);
            occlusion += range_factor * range_factor;
        }
    }

    float visibility = 1.0 - (occlusion / 16.0);
    visibility = clamp(pow(visibility, push_constants.power > 0.0 ? push_constants.power : 1.5f), 0.0, 1.0);

    output.visibility = visibility;
    return output;
}
