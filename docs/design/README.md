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

### PBR & Render Pipeline Performance Overhaul

* **[PBR Opaque vs. Masked Pipeline Separation](proposals/pbr_opaque_masked_pipeline_split.md)**: Details splitting forward PBR lighting into dedicated opaque (zero `discard`, Early-$Z$ writes enabled) and masked pipelines, and omitting the fragment shader (`VK_NULL_HANDLE`) for opaque Z-prepasses and shadow cascades to enable hardware double-rate depth rasterization.
* **[Bindless Fallback Textures & Branchless Material Sampling](proposals/bindless_fallback_textures.md)**: Details pre-allocating reserved global descriptor slots (white, flat normal, neutral metallic-roughness, black emissive) and load-time remapping to eliminate 5 dynamic branches per fragment, plus debug magenta checkerboard fallbacks.
* **[CSM Hardware PCF & Shadow Overhaul](proposals/csm_hardware_pcf_shadows.md)**: Details upgrading CSM shadow sampling to Vulkan hardware depth comparison (`SampleCmpLevelZero`), replacing the 9-tap manual ALU loop with a 4-tap Poisson disk kernel (16 bilinearly filtered samples), and precomputing atlas dimensions to eliminate runtime `GetDimensions()` calls.
* **[OpenPBR Codegen & Register Pressure Optimization](proposals/openpbr_codegen_register_optimization.md)**: Details merging metallic-roughness sampling into a single texture instruction, utilizing hardware sRGB views (`VK_FORMAT_R8G8B8A8_SRGB`), reusing normalized vectors and hardware `SV_IsFrontFace`, stripping shadow debug branching in release builds, and pruning dead slab outputs to drop live registers from 62 down to $\le 48$ for double GPU warp occupancy.
