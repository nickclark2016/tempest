# Proposal: Cascaded Shadow Map Hardware PCF & Sampling Overhaul

## Status
Implemented

## Context
In GPU execution profiles, line 72 of [`pbr.slang`](file:///d:/Code/tempest/engine/runtime/render/system/shaders/raster/pbr.slang#L72) calls [`sample_csm_shadow_info`](file:///d:/Code/tempest/engine/runtime/render/system/shaders/common/lighting.slang#L84-L156), which is responsible for **6,062 samples**—representing **40.3% of the entire PBR fragment shader runtime**.

A detailed instruction-level breakdown in [`lighting.slang`](file:///d:/Code/tempest/engine/runtime/render/system/shaders/common/lighting.slang) exposes multiple compounding bottlenecks:

1. **Dynamic Image Dimension Queries**: Lines 136–137 query atlas dimensions at runtime via `shadow_atlas.GetDimensions(atlas_width, atlas_height)` and compute reciprocal floating-point divisions (`1.0 / float(atlas_width)`) on every fragment. This single construct costs **1,479 samples** (815 `LGSB` stalls, 204 `Long Scoreboard` stalls).
2. **Software 3x3 PCF Loop with Dynamic Branching**: Lines 140–152 unroll 9 manual texture fetches (`shadow_atlas.SampleLevel(linear_sampler, sample_uv, 0).r`) followed by an ALU comparison `if (current_depth >= sampled_depth - depth_bias) shadow += 1.0;`. Line 148 alone incurs **2,237 samples** (1,907 `LGSB` stalls, 1,670 dependency-attributed samples).
3. **Heavy Per-Pixel Struct Copies**: Line 109 (`ShadowCascadeData cascade = shadow_data.cascades[cascade_idx];`) copies 96 bytes (`float4x4` + vectors) from Physical Storage Buffer memory into local registers (**663 samples**, 2,850 dependency-attributed samples).
4. **Warp-Divergent Frustum Bounds Testing**: Lines 119–122 execute 6 chained scalar comparisons (`light_ndc.x < -1.0 || light_ndc.x > 1.0 ...`), causing divergence across warp lanes near cascade boundaries (**185 samples**).

---

## Proposed Architecture

```mermaid
graph TD
    subgraph Host Shadow Setup
        CPU[renderer::prepare_frame] -->|Precalculate Metrics| PC[Push Constants / Shadow Data]
        PC -->|Pass Width, Height, 1/Width, 1/Height| GPU_PC[Zero-Cost Atlas Texel Size]
        SAMPLER[VkSampler with compareEnable=true] -->|CompareOp: GREATER_OR_EQUAL| GPU_SAMP[Hardware Comparison Sampler]
    end

    subgraph GPU Fragment Shader: csm_shadow
        FRAG[PBR Fragment] -->|World Pos, Depth| SEL[Logarithmic Cascade Selection]
        SEL -->|Direct Matrix Transform| PROJ[Light Clip Space Projection]
        PROJ -->|Hardware 4-Tap Poisson Disk| HW_PCF[4x SampleCmpLevelZero]
        
        HW_PCF -->|Each tap = Hardware 2x2 Bilinear PCF| FILT[16 Depth Samples Filtered]
        FILT -->|Weighted Average| SHADOW[Smooth Penumbra Shadow Factor in [0, 1]]
    end
```

---

## Detailed Design

### 1. Hardware Depth Comparison Sampler (`VkSampler`)

In Vulkan, hardware texture filtering units feature dedicated depth-comparison circuitry that performs bilinear percentage-closer filtering (PCF) in silicon during the texture sample operation.

In [`resource_pool.cpp`](file:///d:/Code/tempest/engine/runtime/render/system/src/resource_pool.cpp), create a dedicated comparison sampler:

```cpp
auto shadow_sampler_info = rhi::sampler_create_info{
    .mag_filter = rhi::filter::linear,
    .min_filter = rhi::filter::linear,
    .mipmap_mode = rhi::sampler_mipmap_mode::nearest,
    .address_mode_u = rhi::sampler_address_mode::clamp_to_edge,
    .address_mode_v = rhi::sampler_address_mode::clamp_to_edge,
    .address_mode_w = rhi::sampler_address_mode::clamp_to_edge,
    .compare_enable = true,
    .compare_op = rhi::compare_op::greater_or_equal, // Reverse-Z: lit if current_depth >= occluder_depth
    .min_lod = 0.0F,
    .max_lod = 0.0F,
};
_shadow_comparison_sampler = _device->create_sampler(shadow_sampler_info);
```

In Slang, this is bound as `SamplerComparisonState`. Calling `SampleCmpLevelZero(comparison_sampler, sample_uv, compare_depth)` returns a hardware-filtered scalar float in $[0.0, 1.0]$. A single tap evaluates a $2\times 2$ depth footprint with zero software ALU comparisons.

---

### 2. Precomputed Atlas Metrics & Sampler Descriptor in Scene & Shadow Data

Rather than consuming precious push constant memory, the precomputed shadow metrics and comparison sampler descriptor index are stored directly in existing GPU uniform/BDA structures written once per frame in [`renderer.cpp`](file:///d:/Code/tempest/engine/runtime/render/system/src/renderer.cpp):

1. **`shadow_comparison_sampler_index` in `scene_constants` (`SceneGlobals` in Slang)**:
   Replaces 8 bytes of existing padding at byte offset 360 (`padding[2]` -> `int32_t shadow_comparison_sampler_index` + `uint32_t padding[1]`). The struct remains **exactly 400 bytes** with zero size increase and zero push constant churn.
2. **`atlas_texel_size` in `directional_shadow_data` (`DirectionalShadowData` in Slang)**:
   Precomputed as `{1.0f / width, 1.0f / height}` on the CPU and placed at byte offset 400 on the same 64-byte L1/L2 cache line as `normal_bias` and `depth_bias` (416 bytes total, 16-byte aligned).

This completely removes runtime dimension queries from fragment shaders:
```slang
// ELIMINATED:
uint atlas_width = 8192;
uint atlas_height = 8192;
shadow_atlas.GetDimensions(atlas_width, atlas_height); // 564 samples
float2 texel_size = float2(1.0 / float(atlas_width), 1.0 / float(atlas_height)); // 915 samples
```
**Immediate Gain: 1,479 samples (10% of total PBR fragment time) eliminated.**

---

### 3. 4-Tap Poisson Disk Hardware PCF Kernel

Replace the unrolled 9-tap software loop with a 4-tap Poisson disk hardware comparison kernel:

```slang
// lighting.slang

// Canonical 4-tap Poisson distribution
static const float2 POISSON_DISK_4[4] = {
    float2(-0.38, -0.62),
    float2( 0.65, -0.45),
    float2(-0.52,  0.55),
    float2( 0.42,  0.68)
};

[ForceInline]
ShadowSampleResult sample_csm_shadow_info(
    Texture2D shadow_atlas,
    SamplerComparisonState comparison_sampler,
    DirectionalShadowData* shadow_data,
    float3 world_pos,
    float3 geom_normal,
    float view_depth)
{
    ShadowSampleResult result;
    result.factor = 1.0;
    result.cascade_index = -1;

    if (shadow_data == nullptr || shadow_data.cascade_count == 0) {
        return result;
    }

    // 1. Cascade Selection
    int cascade_idx = -1;
    [unroll]
    for (uint i = 0; i < shadow_data.cascade_count; ++i) {
        if (view_depth < shadow_data.cascades[i].split_depth) {
            cascade_idx = (int)i;
            break;
        }
    }

    if (cascade_idx < 0) {
        return result; // Beyond shadow distance
    }

    result.cascade_index = cascade_idx;

    // Direct pointer reference: avoids copying 96-byte struct to local registers
    ShadowCascadeData* cascade = &shadow_data.cascades[cascade_idx];

    // 2. Normal Offset Bias
    float3 biased_pos = world_pos + geom_normal * shadow_data.normal_bias;

    // 3. Project into Light Clip Space
    float4 light_clip = mul(cascade.view_proj, float4(biased_pos, 1.0));
    float3 light_ndc = light_clip.xyz / light_clip.w;

    // Fast vectorized bounds check
    if (any(abs(light_ndc.xy) > 1.0) || light_ndc.z < 0.0 || light_ndc.z > 1.0) {
        return result;
    }

    float2 light_uv = light_ndc.xy * 0.5 + 0.5;
    float2 atlas_uv = cascade.uv_offset_scale.xy + light_uv * cascade.uv_offset_scale.zw;
    float compare_depth = saturate(light_ndc.z + shadow_data.depth_bias);

    // 4. Hardware PCF (4 taps * 2x2 bilinear = 16 filtered depth samples)
    float shadow = 0.0;
    const float filter_radius = 1.5;

    [unroll]
    for (int i = 0; i < 4; ++i) {
        float2 offset_uv = atlas_uv + POISSON_DISK_4[i] * shadow_data.atlas_texel_size * filter_radius;
        shadow += shadow_atlas.SampleCmpLevelZero(comparison_sampler, offset_uv, compare_depth);
    }

    result.factor = shadow * 0.25;
    return result;
}
```

---

## Technical Comparison: Current vs. Proposed

| Feature | Current Implementation | Proposed Overhaul |
| :--- | :--- | :--- |
| **Texture Dimensions** | Dynamic query `GetDimensions()` every fragment | Precomputed constants via push constants |
| **Texture Instruction** | 9x `SampleLevel` (Point/Linear fetch) | 4x `SampleCmpLevelZero` (Hardware PCF) |
| **Depth Comparison** | 9x manual ALU `if` statements in shader | Single-cycle hardware comparator on TMU |
| **Effective Filter Footprint** | 9 discrete point samples (blocky penumbra) | $4 \times (2\times 2) = 16$ bilinearly filtered samples |
| **Memory Traffic** | 9 separate texture cache lines | 4 texture cache requests |
| **Register & Instruction Stalls** | 2,237 samples in depth comparison loop | Zero software branch/comparison stalls |
| **Struct Copy Overhead** | 96-byte `ShadowCascadeData` copied to regs | Direct pointer access `&cascades[idx]` |

---

## Expected Performance Gains

- **Recovery of Atlas Query Cycles**: Eliminates **1,479 samples** directly by passing precomputed texel sizes.
- **Hardware Depth Comparison Yield**: Replaces the **2,237 sample** ALU loop with 4 hardware PCF instructions, saving an estimated **1,800+ samples**.
- **Net Impact on PBR Fragment Shader**: Reduces `sample_csm_shadow_info` execution cost from **6,062 samples** down to $\approx 1,500-1,800$ samples—a **$70\%$ reduction in shadow calculation cost** and an immediate **$\approx 25\%$ speedup for the entire PBR pass**.
