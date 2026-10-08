# Tempest Engine Design Documents

This directory contains architectural and mathematical design documents for the Tempest Engine.

## Index of Design Documents

* **[Coordinate Systems & Perspective Projection](coordinate_systems_and_projection.md)**: Details coordinate conventions across world, view, clip, NDC, and screen spaces, the mathematical derivation of the infinite Reverse-$Z$ perspective projection matrix, column-major storage layouts, Vulkan NDC $Y$-inversion, and depth testing configuration.

## Architecture Proposals

* **[Persist Negotiated Surface Format](proposals/surface_format_preservation.md)**: Details persisting negotiated surface format and color space directly within `surface_state` for robust dynamic swapchain recreation.
* **[Decoupled Viewport Dimensions](proposals/decoupled_viewport_dimensions.md)**: Details decoupling 3D offscreen scene render target dimensions from swapchain window dimensions to optimize fillrate, VRAM, and dynamic docking resize in editor workflows.
* **[Multi-Window Presentation Lifecycle](proposals/multi_window_presentation.md)**: Details batch multi-surface rendering, frame flight synchronization, and presentation across multiple top-level OS windows.
* **[Coroutine-Native Job System](proposals/coroutine_job_system.md)**: Details an asynchronous C++20 coroutine job system with topology pinning (P/E cores), coroutine-aware sync, channels, zero-exception expected handling, cross-thread profiler awareness, non-blocking GPU sync points, and concurrent render command recording.
* **[Networked Physics, Client Prediction & Procedural World](proposals/networked_physics_world_generation.md)**: Details a server-authoritative multiplayer physics pipeline (Jolt), input ring-buffer client prediction and reconciliation, remote entity dead reckoning, and a shared deterministic macro/micro procedural world generation model.
* **[Production Multiplayer Architecture Roadmap](proposals/production_networking_architecture_roadmap.md)**: Details the transition from single-server prediction to a production-grade distributed MMO/open-world network stack: chunk-relative coordinates, edge gateways, multi-server spatial meshing, token-based authentication, and ChaCha20-Poly1305 AEAD wire security.
* **[Unified Result-Producing Command Line Parser](proposals/command_line_argument_parser.md)**: Details a strongly-typed, zero-allocation schema-based CLI parser in `tempest::core` that produces validated immutable results rather than mutating pre-existing objects.

### Editor & Scene System

* **[Editor & Scene System (Umbrella)](proposals/editor_and_scene_system.md)**: The shared foundation, key decisions, pre-existing defects fixed, risks, verification matrix and the M1–M9 roadmap for scene saves, prefabs, multi-scene loading, baking, editor authoring with undo, play mode, and the Log and Project panes.
* **[Component Reflection & Injected Type Registry](proposals/component_reflection_type_registry.md)**: A dependency-injected `component_type_registry` that replaces the static type-index map; `reflect<T>` field descriptors; MSVC/Clang type-name normalization; the shape-test and golden-manifest type-identity gate; migration hooks.
* **[Scene & Prefab Format, Prefab Instancing, Baking](proposals/scene_prefab_format_and_bake.md)**: Deterministic `.tscene` / `.tprefab` JSON behind an opaque yyjson writer, file-local entity ids, live-linked per-field prefab overrides with nesting, glTF model prefabs, load/save validation, and the versioned `.tscenebin` bake.
* **[Scene Manager & Multi-Scene Loading](proposals/scene_manager_multi_scene_loading.md)**: One world registry holding many scenes, additive and single loading and unloading, a three-stage decode/resolve/time-sliced-commit pipeline for sync and async loads, and a main-thread executor.
* **[Project & Asset Workspace](proposals/project_asset_workspace.md)**: `.tproject` and committed `.tmeta` sidecars, `.tassetdb` as a derived cache, built-in primitives, `.tmaterial` assets, an OS file watcher, a reverse dependency index, and the asset-removal flow.
* **[Editor Authoring, Undo & Play Mode](proposals/editor_authoring_and_play_mode.md)**: ECS hierarchy primitives, the undoable command layer, multi-select, the cached hierarchy pane, the reflected inspector, Edit/Playing/Paused with registry snapshot/restore, Persist to Scene, the game module contract, and network play.
* **[Editor Log View & Project View](proposals/editor_log_and_project_views.md)**: Logger sink and timestamp extensions, an MPSC log store with a stateful filtered view, and a single-column Project View with drag-to-instantiate.

### PBR & Render Pipeline Performance Overhaul

* **[PBR Opaque vs. Masked Pipeline Separation](proposals/pbr_opaque_masked_pipeline_split.md)**: Details splitting forward PBR lighting into dedicated opaque (zero `discard`, Early-$Z$ writes enabled) and masked pipelines, and omitting the fragment shader (`VK_NULL_HANDLE`) for opaque Z-prepasses and shadow cascades to enable hardware double-rate depth rasterization.
* **[Bindless Fallback Textures & Branchless Material Sampling](proposals/bindless_fallback_textures.md)**: Details pre-allocating reserved global descriptor slots (white, flat normal, neutral metallic-roughness, black emissive) and load-time remapping to eliminate 5 dynamic branches per fragment, plus debug magenta checkerboard fallbacks.
* **[CSM Hardware PCF & Shadow Overhaul](proposals/csm_hardware_pcf_shadows.md)**: Details upgrading CSM shadow sampling to Vulkan hardware depth comparison (`SampleCmpLevelZero`), replacing the 9-tap manual ALU loop with a 4-tap Poisson disk kernel (16 bilinearly filtered samples), and precomputing atlas dimensions to eliminate runtime `GetDimensions()` calls.
* **[OpenPBR Codegen & Register Pressure Optimization](proposals/openpbr_codegen_register_optimization.md)**: Details merging metallic-roughness sampling into a single texture instruction, utilizing hardware sRGB views (`VK_FORMAT_R8G8B8A8_SRGB`), reusing normalized vectors and hardware `SV_IsFrontFace`, stripping shadow debug branching in release builds, and pruning dead slab outputs to drop live registers from 62 down to $\le 48$ for double GPU warp occupancy.
