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
