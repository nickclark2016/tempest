# RHI, Vulkan & Render Graph Guidelines

This document provides deep-dive specifications, post-mortem rationales, and code conventions for the RHI, Vulkan backend, Slang shaders, and Render Graph DAG.

---

## 1. Frame Flight Timeline Synchronization & Encapsulation
* **Single Source of Truth**: The `renderer` is the single source of truth for frame flight timeline synchronization. It internally manages per-flight-slot timeline semaphores matching `pool_config.frames_in_flight`.
* **Host Synchronization**: `renderer::prepare_frame()` automatically performs host synchronization (`wait_for_sync`) on the active flight slot before mutating `resource_pool` or transient allocator state. Higher-level engine contexts (`standalone_engine_context`, `editor_engine_context`) must **never** manually track timeline values or execute timeline host waits.
* **Unified UI Pass Integration**: UI overlay rendering (e.g. ImGui) must be integrated directly into the `renderer` Render Graph DAG via `prepare_frame(..., ui_callback)` rather than executed in a secondary ad-hoc queue submission. This allows the Render Graph `barrier_solver` to solve all image layout transitions and pipeline barriers seamlessly in a single unified execution.

---

## 2. Swapchain & Semaphore Reuse
* **Binary Semaphore Indexing**: When presenting swapchain images with binary semaphores, index render/presentation semaphores **per swapchain image** (or allocate per acquired image index) rather than per frame-in-flight slot. This guarantees that the semaphore is idle before re-signaling on submission.
* **Deferred Swapchain Deletion**: For RHI GPU resources that may be in-flight across frames (such as `render_surface` swapchains), avoid mutable in-place `recreate()` methods. Prefer explicit creation taking an `old_*` handover hint and deferred deletion of the old resource via higher-level engine frame retirement queues.

---

## 3. Render Graph Barrier Solver & Layout Tracking
* **Persistent Layout Tracking**: When resources undergo external or post-batch transitions (such as swapchain images transitioned to `image_layout::present` during presentation), explicitly record the new layout into the barrier solver's persistent state table (`set_texture_state`). This prevents layout mismatch validation errors on subsequent frames.
* **Cross-Frame Synchronization**: For cross-frame and temporal resources, evaluate both `was_written` (prior frame write access) and `is_written` when solving barriers to ensure GPU write caches are properly flushed before downstream reads.
* **Transient Eviction & Surface-Relative Sizing**: Viewport- and surface-dependent transient render targets in render passes must be declared using `rg_texture_size::surface_relative(...)` rather than fixed absolute pixel dimensions. Transient allocators must track inactivity across flight cycles (`unused_cycles`) and immediately evict mismatched surface-relative textures on their next idle flight cycle during resolution changes to prevent unbounded VRAM growth. In-flight active texture descriptor tables must not be cleared during resize callbacks while UI frame drawing is in progress.

---

## 4. Pipeline Barriers & UI Sampling Synchronization
* **Offscreen Render Target Sampling**: When offscreen 3D render targets (such as `TonemappedColorTarget`) are sampled by UI passes (such as ImGui bindless textures), explicitly record pipeline barriers transitioning the texture from color attachment output write to fragment shader read before recording the UI pass.
* **Synchronous Frame Dimension Updates**: In editor and tool harnesses, always evaluate UI window logic and viewport dimension updates (`on_paint()`) before executing 3D scene rendering so camera matrices and render target sizes update synchronously without 1-frame latency.
* **Mipmap Pre-Blit Synchronization**: When generating mipmap chains via blit fallback after buffer-to-image texture upload (`copy_buffer_to_texture`), always record a pipeline barrier transitioning uploaded source mip levels from `pipeline_stage::copy` write access to `pipeline_stage::blit` read access before issuing `blit_texture` commands to prevent read-after-write hazards.

---

## 5. Slang Shaders & Vulkan Bindless Conventions
* **Unbounded Arrays**: In Slang shaders, decorate unbounded arrays with explicit `[[vk::binding(binding, set)]]` attributes to avoid compiler warnings when targeting Vulkan SPIR-V.
* **Descriptor Buffers**: When using `VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT`, omit `VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT` from binding flags (descriptor buffers are inherently update-after-bind).
* **Descriptor Offsets**: Only record `vkCmdSetDescriptorBufferOffsetsEXT` for `VK_PIPELINE_BIND_POINT_GRAPHICS` on command lists associated with graphics queue families.

---

## 6. Programmable Vertex Pulling with BDA
When implementing programmable vertex pulling shaders with Slang and Vulkan Buffer Device Address (BDA):
* **0-Based Vertex ID**: Slang maps `SV_VertexID` to `gl_VertexIndex - BaseVertex` (0-based per draw). When drawing concatenated draw lists or batches with distinct vertex offsets, do **NOT** rely on `cmd.draw_indexed`'s `vertex_offset` parameter.
* **Direct Device Addresses**: Pass the exact byte-offset GPU device address directly via push constants or uniforms:
  ```cpp
  auto gpu_address = buffer_gpu_address + vertex_offset * sizeof(Vertex);
  ```
  and pass `0` for `vertex_offset` in `cmd.draw_indexed`.
* **Scalar Stride Arithmetic for Packed Structs**: For packed C++ vertex structures (such as `ImDrawVert` 20-byte stride: 2x float pos, 2x float uv, 1x uint32 col = 5 uints), avoid high-level struct pointer arithmetic in Slang (which aligns structs to 8 or 16 bytes). Instead, use explicit byte/scalar arithmetic (`vertex_id * 5` for 5 uints) to prevent stride mismatch.

---

## 7. Vulkan Timestamp Queries & Pass Profiling
* **Pipeline Stages for Start Timestamps**: Do **NOT** use `pipeline_stage::top_of_pipe` for start timestamps in multi-pass command buffers. `TOP_OF_PIPE` triggers as soon as the GPU command processor parses the command packet, causing all subsequent passes in a queue batch to share identical start timestamps and accumulate prior passes' execution durations.
* **Record After Barriers**: Record start timestamps using `pipeline_stage::bottom_of_pipe` (or `pipeline_stage::all_commands`) **after** pre-pass pipeline barriers so start timestamps reflect when preceding GPU execution and barrier flushes finish.
* **Query Exact Written Count**: When reading back query results via `get_query_pool_results`, only query the exact count of queries written (`recorded_timestamp_count`), never the full query pool capacity (`timestamp_count`). Requesting unwritten queries in the pool range causes `vkGetQueryPoolResults` without `VK_QUERY_RESULT_WAIT_BIT` to return `VK_NOT_READY` and silently drop query readbacks.

---

## 8. Vulkan Debug Utils & Object Naming Synchronization
* **Host Synchronization Requirement**: Per the Vulkan specification for `vkSetDebugUtilsObjectNameEXT`, host access to `pNameInfo->objectHandle` **must be externally synchronized**.
* **Prohibition in Concurrent Command Recording**: Never call `dev.set_debug_name()` or `set_object_name()` inside concurrent pass-recording worker tasks (such as render graph `record_task`). Concurrent calls on shared handles cause data races in Vulkan Validation Layers (VVL) and driver state tables (`nvoglv64.dll`), leading to access violations.
* **Sequential Naming on Creation & Reuse**:
  - Name newly allocated textures and buffers sequentially on the host/main thread upon initial creation (`dev.create_texture`, `dev.create_buffer`).
  - When the transient allocator reuses or aliases pooled physical resources for a registered resource with a different name, update the debug name sequentially during `transient_allocator::allocate()` on the main thread prior to dispatching pass recording tasks.

---

## 9. Shadow Atlas Optimization: Scissor Clears, ZBC & Single-Texture Allocation
* **Scissor-Only Clears (`load_op::dont_care`)**:
  - Large atlas attachments (such as shadow maps) must not use `load_op::clear`, which wastes gigabytes of memory bandwidth clearing unused or dormant tiles.
  - Set `.depth_load_op = rhi::load_op::dont_care` on the render pass depth attachment.
  - Issue explicit scissor clears (`pass_cmd.clear_depth_attachment` -> `vkCmdClearAttachments`) targeting only the bounding rect of each active cascade/tile viewport inside the render pass execution callback.
* **Hardware Fast Depth Clear (ZBC) Layout Preservation**:
  - Reading depth/shadow textures in `image_layout::general` can decompress depth metadata or disable hardware Fast Depth Clear / Zero Bandwidth Clear (ZBC).
  - Always transition depth textures to `rhi::image_layout::depth_stencil_read_only_optimal` when read by downstream fragment passes (PBR opaque, masked, transparency resolve).
  - Ensure the corresponding sampled image descriptor is written with `image_layout::depth_stencil_read_only_optimal`.
* **Effective Cascade Resolution Clamping & Allocator Spacing**:
  - When packing multiple cascades into an atlas with border padding ($P$), spacing between adjacent tiles is $2P$. Total span is $\text{grid\_dim} \times (\text{res} + 2P)$.
  - For $4096$ cascades with $P=4$ in a 2x2 grid, total width is $2 \times (4096 + 8) = 8208\text{px}$, which exceeds 8192 and triggers power-of-two growth to 16384 (16K).
  - Clamp cascade resolutions to the exact available dimension ($(\text{max\_dim} / \text{grid}) - 2P = 4088$ for 8K) rather than standard power-of-two increments.
  - Both atlas sizing and the pass allocator/render loop (`shadow_pass.cpp`) must consume the **effective** clamped cascade resolution to avoid tile allocation failures.
* **Single Persistent Texture Lifecycle vs. Double Buffering**:
  - Resources written and read within the same frame cycle (intra-frame dependencies such as shadow atlases) must use a single persistent physical texture (`rhi::texture_handle`) imported into the graph rather than double-buffered temporal textures, eliminating 50% of VRAM overhead.
  - Reserve double buffering (`temporal_texture`) exclusively for inter-frame temporal dependencies (e.g. TAA history, SSAO history).
  - When persistent imported textures resize, route old handles through the deferred retirement queue (`enqueue_texture_retirement`) tied to timeline semaphores rather than destroying them immediately while in flight.
