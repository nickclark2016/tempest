# Project Guidelines and Rules

## Code Style & Architecture

### 1. Prefer Engine Standard Library Types (`tempest::`) over `std::`
Avoid using standard library types from namespace `std::` or headers like `<cstdint>` when engine-native equivalents exist in namespace `tempest::` and `<tempest/int.hpp>`.
- Prefer `tempest::optional` and `tempest::nullopt` over `std::optional` and `std::nullopt`.
- Prefer `tempest::vector`, `tempest::string_view`, `tempest::unique_ptr`, `tempest::make_unique` over `std::` counterparts.
- Prefer `<tempest/int.hpp>` (`tempest::uint32_t`, `tempest::int32_t`, etc.) over `<cstdint>`.
- Prefer `tempest::inplace_vector<T, N>` over `tempest::vector` for small fixed-capacity collections with dynamic runtime counts (e.g., history buffers, inline batches).
- Use `tempest::min`, `tempest::max`, and `tempest::clamp` directly from namespace `tempest::` (from `<tempest/algorithm.hpp>` and `<tempest/math_utils.hpp>`).

### 2. Adhere to AAA (Almost Always Auto) Rules
Use `auto` for local variable declarations with explicit initializations.
- Example: `auto found_active = tempest::optional<ecs::entity>();`
- Example: `auto to_remove = tempest::vector<ecs::entity>();`
- Avoid uninitialized or explicitly typed declarations like `tempest::optional<ecs::entity> found_active;`.

### 3. Prefer Template Argument Deduction
Use template argument deduction for function calls rather than specifying explicit template arguments whenever the compiler can infer the type.
- Example: `registry->assign(target, graphics::active_camera_component{});`
- Avoid: `registry->assign<graphics::active_camera_component>(target, {});`.

### 4. Assume Valid Invariants Over Defensive Null Checks
Core engine components and required subsystems (e.g., `camera_system` on a `renderer`) should be represented as non-null references. Avoid defensive null pointer checks or fallback branches for ill-formed states; assume input invariants are valid.
- Example: `get_camera_system()` returns `camera_system&` instead of `camera_system*`.
- Omit redundant null checks like `if (pbr_inputs.camera_sys == nullptr)` when building required engine dependencies.

### 5. Deferred Deletion over Mutable In-Place Recreation
For RHI GPU resources that may be in-flight across frames (such as `render_surface` swapchains), avoid mutable in-place `recreate()` methods. Prefer explicit creation taking an `old_*` handover hint and deferred deletion of the old resource via higher-level engine frame retirement queues.

### 6. Slang & Vulkan Bindless Conventions
- In Slang shaders, decorate unbounded arrays with explicit `[[vk::binding(binding, set)]]` attributes to avoid compiler warnings when targeting Vulkan SPIR-V.
- When using `VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT`, omit `VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT` from binding flags (descriptor buffers are inherently update-after-bind).
- Only record `vkCmdSetDescriptorBufferOffsetsEXT` for `VK_PIPELINE_BIND_POINT_GRAPHICS` on command lists associated with graphics queue families.

### 7. Prefer `[[maybe_unused]]` over `(void)` Casts
Use the standard `[[maybe_unused]]` attribute on unused parameters or variables rather than `(void)` casts in function bodies.
- Example: `virtual auto on_resize([[maybe_unused]] rhi::device& dev) -> void {}`
- Avoid: `(void)dev;` inside function bodies.

### 8. Vulkan Swapchain Binary Semaphore Reuse
When presenting swapchain images with binary semaphores, index render/presentation semaphores per swapchain image (or allocate per acquired image index) rather than per frame-in-flight slot to guarantee the semaphore is idle before re-signaling on submission.

### 9. Render Graph Cross-Frame Synchronization & Layout Tracking
- When resources undergo external or post-batch transitions (such as swapchain images transitioned to `image_layout::present` during presentation), explicitly record the new layout into the barrier solver's persistent state table (`set_texture_state`) to prevent layout mismatch validation errors on subsequent frames.
- For cross-frame and temporal resources, evaluate both `was_written` (prior frame write access) and `is_written` when solving barriers to ensure GPU write caches are properly flushed before downstream reads.
 
### 10. `explicit` Constructor Guidelines
Only mark constructors `explicit` when exactly one argument is required and it is not a special copy/move constructor.
- Use `explicit` for single-parameter non-defaulted constructors (e.g., `explicit resource_pool(rhi::device& dev);`).
- Use `explicit` for constructors where only the first parameter is required and trailing parameters have default values (e.g., `explicit camera_system(ecs::archetype_registry& registry, event::event_registry& events = default_events);`).
- Do NOT use `explicit` on multi-parameter constructors requiring two or more arguments (e.g., `shelf_allocator(uint32_t atlas_width, uint32_t atlas_height, uint32_t padding = 4);`).

### 11. Dynamic Shared Library Lifetime & Destruction Order
When dynamically loading shared libraries (`shared_library::load`) that register callbacks, event listeners, polymorphic objects, or ECS components into engine subsystems, always ensure the engine context, UI context, and registries are destructed **before** the shared library handles unload.
- Encapsulate the engine context and its subsystems in an explicit nested scope (`{ ... }`) within the entrypoint before the shared library handles fall out of scope.
- Never unload a dynamic library while function pointers, lambdas, or vtables originating from that library remain active or registered in engine collections.

### 12. Slang Vertex Pulling & Multi-Batch Draw Offsets
When implementing programmable vertex pulling shaders with Slang and Vulkan Buffer Device Address (BDA):
- Recognize that Slang maps `SV_VertexID` to `gl_VertexIndex - BaseVertex` (0-based per draw). When drawing concatenated draw lists or batches with distinct vertex offsets, do NOT rely on `cmd.draw_indexed`'s `vertex_offset` parameter.
- Instead, pass the exact byte-offset GPU device address directly via push constants or uniforms (`buffer_gpu_address + vertex_offset * sizeof(Vertex)`) and pass `0` for `vertex_offset` in `cmd.draw_indexed`.
- For packed C++ vertex structures (such as `ImDrawVert` 20-byte stride), avoid high-level struct pointer arithmetic in Slang (which aligns structs to 8 or 16 bytes); use explicit byte/scalar arithmetic (`vertex_id * 5` for 5 uints) to prevent stride mismatch.

### 13. ECS Hierarchy Traversal for Scene & Prefab Loading
When ingesting, instantiating, or uploading entity hierarchies (such as glTF models or composite prefabs) to GPU memory:
- Do not assume renderable components (`mesh_component`, `material_component`, `renderable_component`) reside on root entities.
- Always recursively traverse `ecs::relationship_component<ecs::entity>` (`first_child`, `next_sibling`) to discover and upload all child entities, submeshes, and material references.

### 14. Avoid `shared_ptr` / Prefer Explicit Ownership Semantics
Do not use `std::shared_ptr` or `tempest::shared_ptr` for resource management, subsystem lifecycles, or architectural problem-solving across the engine codebase.
- Shared ownership semantics obscure object lifetime boundaries, introduce atomic reference-counting overhead, and complicate deterministic destruction order.
- Prefer explicit unique ownership (`tempest::unique_ptr` / `tempest::make_unique`), RAII scope management, explicit registration/unregistration cleanup methods, non-owning raw pointers/references with well-defined parent-child lifetimes, or generational indices (`ecs::entity` with `is_valid` validation).
- When analyzing memory management issues, proposing fixes, or refactoring code, never suggest or introduce `shared_ptr` as a solution.

### 15. Render Graph Transient Resource Eviction & Surface-Relative Targets
- Viewport- and surface-dependent transient render targets in render passes must be declared using `rg_texture_size::surface_relative(...)` rather than fixed absolute pixel dimensions.
- Transient resource allocators must track inactivity across frame-in-flight cycles (`unused_cycles`) and immediately evict mismatched surface-relative textures on their next idle flight cycle during resolution changes to prevent unbounded VRAM growth.
- In-flight active texture descriptor tables must not be cleared during resize callbacks while UI frame drawing is in progress.

### 16. Binary Chunk Arena & Asset Packing Isolation
- In binary asset databases, serialization chunk arenas, or pooled memory streams, never share packing cursor state (`current_chunk_used`, `current_chunk_capacity`) across dedicated large-asset allocations and small-asset packing buffers.
- Always isolate small-asset packing into dedicated arena buffers so appending large standalone chunks cannot corrupt packing offsets or cause small object payloads to overwrite large asset buffers in memory.

### 17. Offscreen Render Target UI Sampling & Pipeline Synchronization
- When offscreen 3D render targets (such as `TonemappedColorTarget`) are sampled by UI passes (such as ImGui bindless textures), explicitly record pipeline barriers transitioning the texture from color attachment output write to fragment shader read before recording the UI pass.
- In editor and tool harnesses, always evaluate UI window logic and viewport dimension updates (`on_paint()`) before executing 3D scene rendering so camera matrices and render target sizes update synchronously without 1-frame latency.

### 18. Renderer Encapsulated Frame Flight Synchronization & UI Pass Integration
- The `renderer` is the single source of truth for frame flight timeline synchronization. It internally manages per-flight-slot timeline semaphores matching `pool_config.frames_in_flight`.
- `renderer::prepare_frame()` automatically performs host synchronization (`wait_for_sync`) on the active flight slot before mutating `resource_pool` or transient allocator state. Higher-level engine contexts (`standalone_engine_context`, `editor_engine_context`) must NOT manually track timeline values or execute timeline host waits.
- UI overlay rendering (e.g. ImGui) must be integrated directly into the `renderer` Render Graph DAG via `prepare_frame(..., ui_callback)` rather than executed in a secondary ad-hoc queue submission. This allows the Render Graph `barrier_solver` to solve all image layout transitions and pipeline barriers seamlessly in a single unified execution.

### 19. Texture Mipmap Generation Pre-Blit Synchronization
- When generating mipmap chains via blit fallback after buffer-to-image texture upload (`copy_buffer_to_texture`), always record a pipeline barrier transitioning uploaded source mip levels from `pipeline_stage::copy` write access to `pipeline_stage::blit` read access before issuing `blit_texture` commands to prevent read-after-write hazards.

## Workflow & Build Guidelines

### Iterative Milestone Execution & Sync Gates
When executing multi-milestone plans or tasks with sync gates:
- Execute strictly **one milestone per turn**.
- After completing a milestone's code changes and verifying its automated tests, **immediately stop calling tools** to yield the turn and report test results.
- **Never proceed to subsequent milestones** until the user explicitly reviews the current milestone and gives approval to proceed.

### Test Case & Section Documentation
Whenever adding or updating test cases:
- Add descriptive documentation comments (e.g., `/// @brief ...`) above every test function detailing the exact behavior, invariant, or edge case under test.
- Use clear inline comments and numbered steps (`// 1. Setup ...`, `// 2. Act ...`, `// 3. Assert ...`) to demarcate test sections and expectations.
- Group related test cases within files using structured section banners.

### Build & Test Commands (Windows Clang)
- **Premake Generation**:
  `premake5 ninja --cc=clang --shared-engine --shell=posix --rhi-vulkan`
- **Build Tests Target**:
  `$env:PATH += ';C:\Program Files\Git\bin'; ninja -C build/ninja rhi-vk-tests render-graph-tests`
- **Build Examples Target**:
  `$env:PATH += ';C:\Program Files\Git\bin'; ninja -C build/ninja rhi-examples`
- **Run Test Binary**:
  `.\bin\Debug\windows-clang\rhi-vk-tests.exe`
  `.\bin\Debug\windows-clang\render-graph-tests.exe`
- **Run Examples Binary**:
  `.\bin\Debug\windows-clang\rhi-examples.exe --list`
  `.\bin\Debug\windows-clang\rhi-examples.exe --example triangle`

### Commit Messages
- Commit message suggestions must always be a single line under 80 characters.

