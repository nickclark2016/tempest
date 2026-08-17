# Project Guidelines and Rules

## Code Style & Architecture

### 1. Prefer Engine Standard Library Types (`tempest::`) over `std::`
Avoid using standard library types from namespace `std::` when engine-native equivalents exist in namespace `tempest::`.
- Prefer `tempest::optional` and `tempest::nullopt` over `std::optional` and `std::nullopt`.
- Prefer `tempest::vector`, `tempest::string_view`, `tempest::unique_ptr`, `tempest::make_unique` over `std::` counterparts.

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

## Workflow & Build Guidelines

### Build & Test Commands (Windows Clang)
- **Premake Generation**:
  `premake5 ninja --cc=clang --shared-engine --shell=posix --rhi-vulkan`
- **Build Ninja Target**:
  `$env:PATH += ';C:\Program Files\Git\bin'; ninja -C build/ninja rhi-vk-tests`
- **Run Test Binary**:
  `.\bin\Debug\windows-clang\rhi-vk-tests.exe`

### Commit Messages
- Commit message suggestions must always be a single line under 80 characters.
