# Proposal: Bindless Fallback Textures & Branchless Material Sampling

## Status
Proposed

## Context
In [`material.slang`](file:///d:/Code/tempest/engine/runtime/render/system/shaders/common/material.slang), fragment shader execution contains multiple conditional branches to verify whether an asset has an assigned texture or relies solely on constant scalar/vector factors:

- Line 72: `if (mat_state.mat.metallic_roughness_map_id < 0)` (**125 samples**)
- Line 84: `if (mat_state.mat.base_texture_id >= 0)` (**186 samples**)
- Line 104: `if (mat_state.mat.metallic_roughness_map_id < 0)` (**20 samples**)
- Line 113: `if (mat_state.mat.normal_map_id < 0)` (**126 samples**)
- Line 159: `if (mat_state.mat.emissive_texture_id >= 0)` (**249 samples**)
- Line 129: `if (mat_state.mat.transmission_texture_id >= 0)` (**23 samples**)

### The Performance Problem
1. **Warp Divergence**: A GPU warp consists of 32 threads executing in lockstep. In scenes where adjacent triangles or geometry within a screen tile feature diverse materials (e.g., one surface has a normal map and metallic-roughness map while an adjacent surface is flat/untextured), threads in the same warp take opposing branches. Both branch paths must be serially executed with thread lane masks disabled, cutting arithmetic throughput by up to $50\%$.
2. **Instruction Cache & Register Lifetimes**: Dynamic branching introduces jump targets and label blocks, preventing the compiler from hoisting texture sample instructions ahead of dependent arithmetic to hide memory access latency.
3. **Branch Resolving Stalls**: The profile reports significant `Branch Resolving` and `Short Scoreboard` stall cycles specifically on texture check boundaries.

---

## Proposed Architecture

```mermaid
graph TD
    subgraph CPU Resource Pool Initialization
        RP[resource_pool::init] -->|Allocate Slot 0| T0[1x1 White Texture RGBA8]
        RP -->|Allocate Slot 1| T1[1x1 Flat Normal [128, 128, 255, 255]]
        RP -->|Allocate Slot 2| T2[1x1 Neutral Metallic-Roughness [0, 255, 255, 0]]
        RP -->|Allocate Slot 3| T3[1x1 Black Texture RGBA8]
        RP -->|Allocate Slot 4| T4[16x16 Magenta/Black Checkerboard]
    end

    subgraph CPU Material Upload Remapping
        MAT[Gltf / Material Import] -->|Check Texture IDs| REMAP{tex_id < 0?}
        REMAP -->|Release Build: Yes| DEF[Assign Reserved Slot 0..3]
        REMAP -->|Debug Build: Missing| CHK[Assign Slot 4 Checkerboard]
        REMAP -->|Valid Texture ID| LOAD[Assign Uploaded Texture Index]
        DEF --> GPU_MAT[GPU Material Buffer: All Indices >= 0]
        CHK --> GPU_MAT
        LOAD --> GPU_MAT
    end

    subgraph GPU Shader Execution
        GPU_MAT -->|Unconditional Bindless Fetch| FS[Branchless material.slang]
        FS -->|Single Unified Instruction Stream| PIPE[100% Warp Convergence & Memory Coalescing]
    end
```

---

## Detailed Design

### 1. Reserved Bindless Descriptor Allocation in `resource_pool`

In [`resource_pool.hpp`](file:///d:/Code/tempest/engine/runtime/render/system/include/tempest/render_system/resource_pool.hpp), reserve the initial indices of the bindless sampled image array (`Set 1, Binding 0`):

```cpp
namespace tempest::render_system
{
    struct reserved_fallback_texture_indices
    {
        static constexpr int32_t white_opaque            = 0; // 1x1 [255, 255, 255, 255] (Base Color neutral)
        static constexpr int32_t flat_normal             = 1; // 1x1 [128, 128, 255, 255] (Unperturbed normal)
        static constexpr int32_t neutral_metallic_roughness = 2; // 1x1 [0, 255, 255, 0]   (Roughness=1, Metal=1)
        static constexpr int32_t black_opaque            = 3; // 1x1 [0, 0, 0, 0]         (Emissive neutral)
        static constexpr int32_t magenta_checkerboard    = 4; // 16x16 Magenta/Black      (Debug fallback)
        static constexpr uint32_t reserved_count         = 5;
    };
}
```

During engine startup in `resource_pool::init()`, create and upload these 5 canonical textures:
- **Flat Normal**: A $1\times 1$ pixel with RGBA values `[128, 128, 255, 255]`. When unpacked by `tan_normal = sample.rgb * 2.0 - 1.0`, it yields `[0.0, 0.0, 1.0]`. When multiplied by the TBN frame, it yields the interpolated vertex geometric normal without deviation.
- **Neutral Metallic-Roughness**: A $1\times 1$ pixel with green = 255 (Roughness factor = 1.0) and blue = 255 (Metallic factor = 1.0). When sampled and multiplied by `material.roughness_factor` and `material.metallic_factor`, the uniform material parameters determine the surface response exactly as intended.
- **Magenta Checkerboard**: An alternating $16\times 16$ magenta (`#FF00FF`) and black pattern for identifying missing texture assets at visual inspection.

---

### 2. CPU Material Remapping at Load Time

When writing material components into the GPU-mapped material buffer ([`resource_pool.cpp`](file:///d:/Code/tempest/engine/runtime/render/system/src/resource_pool.cpp)):

```cpp
auto remap_texture_index(int32_t texture_id, int32_t default_slot, bool debug_mode) noexcept -> int32_t
{
    if (texture_id >= 0)
    {
        return texture_id;
    }
    if (debug_mode && default_slot == reserved_fallback_texture_indices::white_opaque)
    {
        return reserved_fallback_texture_indices::magenta_checkerboard;
    }
    return default_slot;
}

// During material packing:
gpu_mat.base_texture_id = remap_texture_index(mat.base_texture_id, 
    reserved_fallback_texture_indices::white_opaque, is_debug);
gpu_mat.normal_map_id = remap_texture_index(mat.normal_map_id, 
    reserved_fallback_texture_indices::flat_normal, false);
gpu_mat.metallic_roughness_map_id = remap_texture_index(mat.metallic_roughness_map_id, 
    reserved_fallback_texture_indices::neutral_metallic_roughness, false);
gpu_mat.emissive_texture_id = remap_texture_index(mat.emissive_texture_id, 
    reserved_fallback_texture_indices::black_opaque, false);
```

> [!NOTE]
> Every material in GPU memory is guaranteed to have non-negative, valid descriptor indices for all texture slots.

---

### 3. Fully Branchless Sampling in `material.slang`

With guaranteed descriptor validity, [`material.slang`](file:///d:/Code/tempest/engine/runtime/render/system/shaders/common/material.slang) simplifies to linear, branchless code:

```slang
// material.slang

void fetch_pixel_data_branchless(const MaterialState mat_state, inout PixelData pixel) {
    // 1. Unconditional Base Color Fetch
    half4 tex_color = half4(bindless_textures[(int)mat_state.mat.base_texture_id]
                                .Sample(mat_state.linear_sampler, pixel.uv));
    pixel.diffuse = tex_color * half4(mat_state.mat.base_texture_factor);

    // 2. Unconditional Combined Metallic-Roughness Fetch (Single Texture Instruction)
    half4 mr_sample = half4(bindless_textures[(int)mat_state.mat.metallic_roughness_map_id]
                                .Sample(mat_state.linear_sampler, pixel.uv));
    pixel.roughness = half(mat_state.mat.roughness_factor) * mr_sample.g;
    pixel.metallic  = half(mat_state.mat.metallic_factor)  * mr_sample.b;
    pixel.f0        = lerp(half3(0.04h), pixel.diffuse.rgb, pixel.metallic);

    // 3. Unconditional Normal Map Fetch & Transform
    half3 tan_normal = half3(bindless_textures[(int)mat_state.mat.normal_map_id]
                                 .Sample(mat_state.linear_sampler, pixel.uv).rgb) * 2.0h - 1.0h;
    tan_normal.rg *= half(mat_state.mat.normal_scale);
    half3 normal = tan_normal.x * pixel.geom_tangent + 
                   tan_normal.y * pixel.geom_bitangent + 
                   tan_normal.z * pixel.geom_normal;
    pixel.shading_normal = normalize(normal);
    if (!pixel.front_face) {
        pixel.shading_normal = -pixel.shading_normal;
    }
}

void apply_emissive_branchless(const MaterialState mat_state, inout half4 color, const half2 uv) {
    // Unconditional Emissive Fetch (black fallback yields 0.0 added)
    half4 emissive_sample = half4(bindless_textures[(int)mat_state.mat.emissive_texture_id]
                                      .Sample(mat_state.linear_sampler, uv));
    half3 emissive = half3(mat_state.mat.emissive_factor.rgb) * emissive_sample.rgb;
    color.rgb += emissive;
}
```

---

## Performance Evaluation & Expected Gains

1. **Elimination of Branch Divergence**:
   - In heterogeneous mesh clusters (e.g., metallic objects next to untextured walls), $100\%$ of threads in the warp execute identical instruction sequences.
   - Eliminates over **700 samples** of branch evaluation and resolving overhead observed in the profile.
2. **Instruction Scheduling & Latency Hiding**:
   - Because all texture fetch operations are unconditional, Slang and the SPIR-V compiler can emit all three texture sample instructions (`base_color`, `metallic_roughness`, `normal_map`) back-to-back at the beginning of the fragment shader.
   - The GPU's texture sampling unit processes all three requests concurrently while the warp initiates mathematical setup, hiding texture memory latency behind instruction-level parallelism.
3. **Debugging Usability**:
   - Missing textures show up as vivid magenta checkerboards in Debug builds without requiring a single runtime `if` check in shader assembly.
