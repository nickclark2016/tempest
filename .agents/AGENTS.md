# Project Guidelines and Rules

## Subsystem Reference Guides
Deep-dive specifications, post-mortem rationales, and extended code recipes are maintained in dedicated reference documents. Consult these before modifying the corresponding subsystems:
- **RHI, Vulkan & Render Graph**: [rendering_vulkan.md](file:///home/ntc0531/repos/tempest/.agents/references/rendering_vulkan.md) — Swapchain semaphores, frame timeline encapsulation, barrier solver tracking, Slang BDA vertex offsets, and timestamp queries.
- **Profiler & Telemetry**: [profiler_telemetry.md](file:///home/ntc0531/repos/tempest/.agents/references/profiler_telemetry.md) — `scoped_zone` RAII scopes, chunk recycling, JS safe integer limits, non-blocking socket framing, and UI timeline multi-lane layout.
- **Concurrency & Runtime**: [concurrency_runtime.md](file:///home/ntc0531/repos/tempest/.agents/references/concurrency_runtime.md) — Thread-confinement, worker state, dynamic library unload order, and TSan verification.
- **Architecture, ECS & Assets**: [architecture_assets.md](file:///home/ntc0531/repos/tempest/.agents/references/architecture_assets.md) — ECS hierarchy traversal, ownership models, and binary chunk arena isolation.

---

## Core C++ Language & Dialect

### 1. Engine Standard Library Types (`tempest::`)
Prefer engine-native types over `std::` and `<cstdint>`:
| Use `tempest::` | Instead of `std::` / C | Header / Notes |
| :--- | :--- | :--- |
| `optional`, `nullopt` | `std::optional`, `std::nullopt` | `<tempest/optional.hpp>` |
| `vector`, `string_view` | `std::vector`, `std::string_view` | `<tempest/vector.hpp>`, `<tempest/string_view.hpp>` |
| `unique_ptr`, `make_unique` | `std::unique_ptr`, `std::make_unique` | `<tempest/memory.hpp>` |
| `<tempest/int.hpp>` | `<cstdint>` | `uint32_t`, `int32_t`, `uint64_t`, etc. |
| `inplace_vector<T, N>` | `std::vector` | Small fixed-capacity collections with dynamic runtime counts |
| `non_null<T>` | Raw non-null pointer / reference members | `<tempest/checked.hpp>` |
| `min`, `max`, `clamp` | `std::min`, `std::max`, `std::clamp` | `<tempest/algorithm.hpp>`, `<tempest/math_utils.hpp>` |

### 2. Variable Declarations & Naming
- **AAA (Almost Always Auto)**: Use `auto` for local variable declarations with explicit initialization (e.g. `auto found = tempest::optional<ecs::entity>();`). Avoid uninitialized or explicitly typed declarations.
- **Prefer `const` Locals**: Prefer `const auto` for local variables whenever they are not mutated.
- **Descriptive Variable Names**: Use descriptive variable names; never use single-letter variable names (e.g. use `frame_index` or `entity` rather than `f` or `e`).
- **Template Argument Deduction**: Use template argument deduction for function calls rather than specifying explicit template arguments when types are inferable (e.g. `registry->assign(target, active_camera_component{});`).
- **Unused Entities**: Use standard `[[maybe_unused]]` on unused parameters or variables; never use `(void)` casts.

### 3. Struct & Class Member Guidelines
- **No References as Members**: Never use C++ references (`T&`) as class or struct members. References make types non-assignable/non-movable and obscure lifetime.
- **Non-Nullable Struct Pointers**: Structs and classes containing pointers that must not be null must use `tempest::non_null<T>` (from `<tempest/checked.hpp>`) rather than raw pointers or references.
- **`explicit` Constructors**: Only mark constructors `explicit` when exactly one argument is required and it is not a copy/move constructor. Never mark multi-parameter constructors requiring two or more arguments `explicit`.

### 4. Global Architectural Invariants
- **Assume Valid Invariants Over Defensive Null Checks**: Core engine components and required subsystems (e.g. `camera_system` on a `renderer`) must be represented as non-null references. Avoid defensive null pointer checks or fallback branches for ill-formed states; assume input invariants are valid.
- **Explicit Ownership Semantics (Strict Prohibition of `shared_ptr`)**: Never use `std::shared_ptr` or `tempest::shared_ptr` anywhere in the codebase. Shared ownership obscures object lifetime boundaries, introduces atomic ref-counting overhead, and complicates deterministic destruction. Use explicit unique ownership (`tempest::unique_ptr` / `tempest::make_unique`), RAII scope management, non-owning raw pointers/references with well-defined parent-child lifetimes, or generational indices (`ecs::entity`). Never suggest or introduce `shared_ptr` as a solution when analyzing memory issues, proposing fixes, or refactoring.
- **Strict Prohibition of `thread_local` and Global Variables**: Never introduce `thread_local`, global variables, or static mutable state anywhere in engine runtime libraries. Confine worker state via worker-indexed state structures (e.g. `worker_state` via `find_current_worker()` or `job_context`). Sockets, pools, and singletons must maintain clear RAII lifecycles tied to engine or system instances.

---

## Subsystem Invariant Anchors

### RHI, Vulkan & Render Graph
- **Swapchain Semaphores**: Index render/presentation binary semaphores per swapchain image (or acquired image index), never per frame-in-flight slot. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/rendering_vulkan.md#2-swapchain--semaphore-reuse))
- **Deferred Deletion**: Avoid mutable in-place `recreate()` for in-flight GPU resources; use explicit creation with `old_*` handover hints and deferred deletion via frame retirement queues. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/rendering_vulkan.md#2-swapchain--semaphore-reuse))
- **Bindless Conventions**: In Slang, decorate unbounded arrays with `[[vk::binding(binding, set)]]`. Omit `UPDATE_AFTER_BIND` on descriptor buffers. Record descriptor offsets for `GRAPHICS` only on graphics queue command lists. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/rendering_vulkan.md#5-slang-shaders--vulkan-bindless-conventions))
- **Slang BDA Vertex Pulling**: Slang maps `SV_VertexID` to 0-based index. Do NOT use `cmd.draw_indexed` vertex offsets or packed struct pointer arithmetic; pass exact byte GPU device addresses directly (`buffer_gpu_address + vertex_offset * sizeof(Vertex)`) with 0 offset. Use scalar stride (`vertex_id * 5`) for packed vertices like `ImDrawVert`. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/rendering_vulkan.md#6-programmable-vertex-pulling-with-bda))
- **Timestamp Queries**: Record pass start timestamps using `bottom_of_pipe` (or `all_commands`) *after* pre-pass barriers (never `top_of_pipe`). Read back only the exact `recorded_timestamp_count`, never full query pool capacity. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/rendering_vulkan.md#7-vulkan-timestamp-queries--pass-profiling))
- **Frame Flight Synchronization**: `renderer` is the single source of truth for timeline semaphores; `renderer::prepare_frame()` executes host wait sync (`wait_for_sync`). Higher-level engine contexts must NOT manually track timeline values. UI overlay rendering (ImGui) must be integrated via `prepare_frame(..., ui_callback)` in the Render Graph DAG. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/rendering_vulkan.md#1-frame-flight-timeline-synchronization--encapsulation))
- **Barrier Solver Layout Tracking**: External and post-batch transitions (e.g. `image_layout::present`) must be explicitly recorded in the barrier solver's persistent state table (`set_texture_state`). Evaluate both `was_written` and `is_written` for cross-frame resources. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/rendering_vulkan.md#3-render-graph-barrier-solver--layout-tracking))
- **Transient Resource Eviction**: Declare surface-dependent transient targets with `rg_texture_size::surface_relative(...)`. Evict mismatched targets on their next idle flight cycle during resize; never clear active descriptor tables during resize callbacks while UI painting is in progress. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/rendering_vulkan.md#3-render-graph-barrier-solver--layout-tracking))
- **UI Offscreen Sampling Barrier**: Offscreen render targets sampled by UI passes must transition from color write to fragment read before recording the UI pass. Update UI logic (`on_paint()`) before 3D scene rendering. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/rendering_vulkan.md#4-pipeline-barriers--ui-sampling-synchronization))
- **Mipmap Pre-Blit Barrier**: Transition uploaded mip levels from `pipeline_stage::copy` write to `pipeline_stage::blit` read before issuing `blit_texture`. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/rendering_vulkan.md#4-pipeline-barriers--ui-sampling-synchronization))

### Profiler, Telemetry & Network
- **`scoped_zone` Scope**: Encapsulate top-level CPU profiling zones (`profiler::scoped_zone`) in an explicit nested scope `{ ... }` that terminates *before* invoking `collect_and_broadcast_telemetry()` so frame durations commit to frame $N$, not $N+1$. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/profiler_telemetry.md#1-frame-level-scoped_zone-raii-scope-before-telemetry-capture))
- **Chunk Recycling**: Recycle drained profiler session and arena chunks via `session.recycle_chunks()` rather than destroying them. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/profiler_telemetry.md#2-profiler--chunk-arena-recycling))
- **JS Safe Integer Limits**: Serialize 64-bit IDs for Web/JS within JavaScript's safe integer range ($< 2^{53}-1$, e.g. 32-bit prefix `0x8000'0000ULL`) or as quoted strings. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/profiler_telemetry.md#3-web--json-serialization-number-precision-javascript-max_safe_integer))
- **Socket Send Framing**: Broadcast over non-blocking TCP/WebSocket sockets must loop on partial sends with write readiness polling (`select`/`WSAPoll`); never discard unsent bytes. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/profiler_telemetry.md#4-non-blocking-socket-framing--websocket-transport))
- **Profiler UI Timeline Lanes**: Profiler UI timeline tracks must maintain 3 distinct vertical lanes (Track Header Strip, Frame Header Lane, Call Stack Zone Area below). ([Details](file:///home/ntc0531/repos/tempest/.agents/references/profiler_telemetry.md#5-profiler-ui-timeline-multi-lane-track-hierarchy))

### Runtime, Assets & ECS
- **Dynamic Shared Libraries**: Engine context and registries must be scoped and destructed *before* dynamic library handles unload. Never unload dynamic libraries while callbacks/vtables remain active. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/concurrency_runtime.md#2-dynamic-shared-library-lifetime--destruction-order))
- **ECS Hierarchy Traversal**: Recursively traverse `ecs::relationship_component<ecs::entity>` (`first_child`, `next_sibling`) to discover all child entities and submeshes; do not assume components reside on root entities. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/architecture_assets.md#1-ecs-hierarchy-traversal-for-scene--prefab-loading))
- **Binary Chunk Arena Isolation**: Never share packing cursor state across dedicated large-asset allocations and small-asset packing buffers; isolate small-asset packing into dedicated arena buffers. ([Details](file:///home/ntc0531/repos/tempest/.agents/references/architecture_assets.md#2-binary-chunk-arena--asset-packing-isolation))

---

## Workflow & Development Rules

### 1. Implementation Planning & Test Plan Requirement
When creating implementation plans:
- A comprehensive verification/test plan (automated test targets, manual test cases, and invariant validations) must be explicitly determined as part of the implementation plan itself before starting execution.

### 2. Iterative Milestone Execution & Sync Gates
When executing multi-milestone plans or tasks with sync gates:
- Execute strictly **one milestone per turn**.
- After completing a milestone's code changes and verifying its automated tests, **immediately stop calling tools** to yield the turn and report test results.
- **Never proceed to subsequent milestones** until the user explicitly reviews the current milestone and gives approval to proceed.

### 3. Test Case & Section Documentation
Whenever adding or updating test cases:
- Add descriptive documentation comments (e.g. `/// @brief ...`) above every test function detailing the exact behavior, invariant, or edge case under test.
- Use clear inline comments and numbered steps (`// 1. Setup ...`, `// 2. Act ...`, `// 3. Assert ...`) to demarcate test sections and expectations.
- Group related test cases within files using structured section banners.

### 4. ThreadSanitizer (TSan) for Concurrency Changes
Whenever modifying the job system, thread pool, work queues, coroutines, or sync primitives:
- **Mandatory TSan Verification**: Compile and run test suites with ThreadSanitizer enabled via `premake5 ... --use-tsan`.
- **Target Restriction**: Only run on non-GPU test targets (`job-tests`, `profiler-tests`, `render-graph-tests`, `core-tests`, `ecs-tests`, `event-tests`, `serialization-tests`, `assets-tests`). Do not run GPU hardware driver tests (`rhi-vk-tests`) under TSan.

### 5. Embedded Web Assets Build Integration
- Web assets (`index.html`, `app.js`, `styles.css`) are embedded into `web_assets.cpp` via Premake custom actions driven by Ninja build rules. `premake5.lua` must only invoke `embed_web_assets()` during generation if `web_assets.cpp` is missing.

### 6. Architecture Proposals & Backlog Tracking
- Document deferred ideas and architectural improvements in `docs/design/proposals/<name>.md` and index them in `docs/design/README.md`.
- When asked for next tasks, inspect `docs/design/proposals/` and prioritize by subsystem relevance.

### 7. Build & Test Commands Reference
- **Premake**: `premake5 ninja --cc=clang --shared-engine --shell=posix --rhi-vulkan`
- **Build Tests**: `ninja -C build/ninja rhi-vk-tests render-graph-tests`
- **Run Tests**: `bin/Debug/windows-clang/rhi-vk-tests.exe` (or Linux binary path)
- **Commit Messages**: Single line under 80 characters.
