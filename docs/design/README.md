# Tempest Engine Design Documents

This directory contains architectural and mathematical design documents for the Tempest Engine.

## Index of Design Documents

* **[Coordinate Systems & Perspective Projection](coordinate_systems_and_projection.md)**: Details coordinate conventions across world, view, clip, NDC, and screen spaces, the mathematical derivation of the infinite Reverse-$Z$ perspective projection matrix, column-major storage layouts, Vulkan NDC $Y$-inversion, and depth testing configuration.

## Architecture Proposals

* **[Persist Negotiated Surface Format](proposals/surface_format_preservation.md)**: Details persisting negotiated surface format and color space directly within `surface_state` for robust dynamic swapchain recreation.
* **[Decoupled Viewport Dimensions](proposals/decoupled_viewport_dimensions.md)**: Details decoupling 3D offscreen scene render target dimensions from swapchain window dimensions to optimize fillrate, VRAM, and dynamic docking resize in editor workflows.
* **[Multi-Window Presentation Lifecycle](proposals/multi_window_presentation.md)**: Details batch multi-surface rendering, frame flight synchronization, and presentation across multiple top-level OS windows.
