// pbr_common.slang

import "camera";
import "consts";
import "lighting";
import "material";
import "mesh";
import "tonemap.slang";
import "vertex_pull";

struct SceneGlobals {
    Camera camera;
    float2 screen_size;
    float4 ambient_light;
    Light sun;
    uint point_light_count;
};

struct VertexOutput {
    float4 position         : SV_Position;
    float3 world_position   : Position;
    float3 view_position    : ViewPosition;
    float4 current_position : CurrentPosition;
    float4 prev_position    : PreviousPosition;
    float2 uv               : UV0;
    float3 normal           : Normal;
    float3 tangent          : Tangent;
    float3 bitangent        : Bitangent;
    uint material_id        : Material;
};

struct FragmentOutput {
    float4 color : SV_Target0;
    float2 velocity : SV_Target1;
};

struct PBROutput {
    float4 color;
    float2 velocity;
    float depth;
};

[vk_binding(0, 0)] StructuredBuffer<SceneGlobals> scene;
[vk_binding(1, 0)] StructuredBuffer<uint> vertex_pull_buffer;
[vk_binding(2, 0)] StructuredBuffer<Mesh> meshes;
[vk_binding(3, 0)] StructuredBuffer<ObjectData> objects;
[vk_binding(4, 0)] StructuredBuffer<uint> instance_buffer;
[vk_binding(5, 0)] StructuredBuffer<Material> materials;
[vk_binding(15, 0)] SamplerState linear_sampler;
[vk_binding(16, 0)] Texture2D bindless_textures[BINDLESS_TEXTURE_COUNT];

[vk_binding(0, 1)] StructuredBuffer<Light> lights;
[vk_binding(1, 1)] StructuredBuffer<ShadowMapParameter> shadow_map_params;
[vk_binding(2, 1)] Texture2D shadow_map_mt;

half2 calculate_velocity(half4 new_position, half4 previous_position) {
    previous_position /= previous_position.w;
    previous_position.xy = (previous_position.xy + 1.0h) * 0.5h;
    previous_position.y = 1.0h - previous_position.y;

    new_position /= new_position.w;
    new_position.xy = (new_position.xy + 1.0h) * 0.5h;
    new_position.y = 1.0h - new_position.y;

    return (new_position - previous_position).xy;
}

VertexOutput compute_vertex(uint vertex_id, uint instance_id) {
    uint iid = instance_buffer[instance_id];
    ObjectData instance = objects[iid];

    Vertex v = pull_vertex(vertex_pull_buffer, meshes[instance.mesh_id], vertex_id);

    float4 world_pos = mul(instance.model_matrix, float4(v.position, 1.0));
    float4 view_pos = mul(scene[0].camera.view, world_pos);
    float4 clip_pos = mul(scene[0].camera.projection, view_pos);

    float4 prev_world_pos = mul(instance.prev_model_matrix, float4(v.position, 1.0));
    float4 prev_clip_pos = mul(scene[0].camera.prev_projection, mul(scene[0].camera.prev_view, prev_world_pos));

    float3x3 model_3x3 = (float3x3) instance.model_matrix;
    float3x3 t_inv_model_3x3 = (float3x3) instance.transpose_inv_model_matrix;

    float3 geom_normal = normalize(mul(t_inv_model_3x3, v.normal));
    float3 geom_tangent = normalize(mul(model_3x3, v.tangent.xyz));
    float3 geom_bitan = normalize(cross(geom_normal, geom_tangent)) * v.tangent.w;

    VertexOutput result;

    result.position = clip_pos;
    result.current_position = clip_pos;
    result.prev_position = prev_clip_pos;
    result.world_position = world_pos.xyz;
    result.view_position = view_pos.xyz;
    result.uv = v.uv0;
    result.normal = geom_normal;
    result.tangent = geom_tangent;
    result.bitangent = geom_bitan;
    result.material_id = instance.material_id;

    return result;
}

PBROutput compute_fragment(VertexOutput input) {
    LightingState lighting;
    lighting.sun = scene[0].sun;
    lighting.ambient_color_factor = scene[0].ambient_light.rgb;
    lighting.point_and_spot_lights = lights;
    lighting.shadow_map_mt = shadow_map_mt;
    lighting.shadow_map_params = shadow_map_params;
    lighting.shadow_map_sampler = linear_sampler;
    lighting.num_point_and_spot_lights = scene[0].point_light_count;

    MaterialState material;
    material.mat = materials[input.material_id];
    material.linear_sampler = linear_sampler;

    PixelData pd;
    pd.uv = half2(input.uv);
    pd.geom_tangent = half3(input.tangent);
    pd.geom_bitangent = half3(input.bitangent);
    pd.geom_normal = half3(input.normal);
    
    float3 view_dir = normalize(scene[0].camera.eye.xyz - input.world_position);
    pd.front_face = dot(view_dir, input.normal) > 0.0;

    fetch_pixel_data(material, pd, bindless_textures);

    float4 color = evaluate_material(pd, material, lighting, scene[0].camera, input.world_position, input.view_position, bindless_textures);
    color.rgb = uncharted_2(color.rgb * 4.5);
    color.rgb = color.rgb * (1.0f / uncharted_2((11.2f).xxx));

    PBROutput output;

    output.color = color;
    output.velocity = calculate_velocity(half4(input.current_position), half4(input.prev_position));
    output.depth = compute_depth(input.current_position);
    
    if (material.mat.type == MaterialType::TRANSMISSIVE) {
        output.color.a = material.mat.transmission_factor;
    }

    return output;
}

float compute_depth(float4 clip_pos) {
    float f_ndc_depth = clip_pos.z / clip_pos.w;
    return f_ndc_depth;
}

