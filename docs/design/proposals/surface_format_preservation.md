# Proposal: Persist Negotiated Surface Format in Surface State

## Status
Proposed

## Context
When a window surface is initially registered with the `render_system::renderer` via `register_surface()`, the renderer queries surface capabilities (`get_surface_capabilities()`) and negotiates a matching `rhi::surface_format` (favoring `rhi::render_surface_format::bgra8_srgb` / `rgba8_srgb` with `surface_color_space::srgb_nonlinear`).

Currently, when swapchain recreation is triggered on resize or window state change in `renderer::begin_frame()`, the swapchain recreation path queries `surf->render_surface->get_format()` and reconstructs a new `rhi::surface_format` struct assuming `srgb_nonlinear`.

## Proposed Solution

Store the full `rhi::surface_format` directly inside `surface_state`:

```cpp
struct surface_state
{
    window_handle window{null_window_handle};
    rhi::raw_surface_handle raw_surface{};
    unique_ptr<rhi::render_surface> render_surface{};
    rhi::surface_format surface_format{}; // Persisted negotiated format + color space
    uint32_t width{0};
    uint32_t height{0};
    rhi::present_mode present_mode{rhi::present_mode::vsync};
    bool need_recreate{false};
    vector<rhi::semaphore_handle> acquire_semaphores{};
    vector<rhi::semaphore_handle> render_semaphores{};
    optional<rhi::swapchain_image> current_sc_image{nullopt};
    rhi::semaphore_handle current_acquire_semaphore{};
    rhi::semaphore_handle current_render_semaphore{};
};
```

### Benefits:
1. **Deterministic Color Space Preservation**: Guarantees that any negotiated color space (HDR10, scRGB, display P3, or sRGB nonlinear) remains consistent across dynamic swapchain resizing without needing fallback heuristics.
2. **Simplified Recreate Logic**: `begin_frame()` simply passes `surf->surface_format` into `rhi::render_surface_desc` without re-evaluating format preferences.
