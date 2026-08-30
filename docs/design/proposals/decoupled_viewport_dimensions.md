# Proposal: Decoupled Offscreen Viewport Render Dimensions

## Status
Proposed

## Context
Currently, `renderer::render_frame()` passes the swapchain window dimensions (`w, h`) to `renderer::prepare_frame()`. As a consequence, all surface-relative Render Graph textures (`TonemappedColorTarget`, `_hdr_color_target`, `_depth_target`, SSAO, and transparency targets) are allocated at the full swapchain window resolution (e.g., $1920 \times 1080$), even when the docked ImGui 3D viewport window occupies a smaller subset of the screen (e.g., $1280 \times 720$).

## Proposed Architecture

```mermaid
graph TD
    subgraph Current Architecture
        A1[Swapchain Window 1920x1080] -->|Sized by Window| B1[3D Scene Render Targets 1920x1080]
        B1 -->|Render Full Res| C1[TonemappedColorTarget 1920x1080]
        C1 -->|ImGui ui::image Scaled Sample| D1[Docked Viewport 1280x720]
        D1 -->|Composite UI| E1[Swapchain Present 1920x1080]
    end

    subgraph Proposed Decoupled Architecture
        A2[Docked Viewport Resize 1280x720] -->|Explicit Viewport Size| B2[3D Scene Render Targets 1280x720]
        B2 -->|Exact Resolution| C2[TonemappedColorTarget 1280x720]
        C2 -->|1:1 Pixel Sampling| D2[Docked Viewport 1280x720]
        D2 -->|Composite UI| E2[Swapchain Present 1920x1080]
    end
```

## Detailed Design

### 1. Viewport Dimension Separation in `renderer::prepare_frame`

Enhance `renderer::prepare_frame` to accept both an offscreen 3D scene resolution (`render_width, render_height`) and the swapchain target resolution (`swapchain_width, swapchain_height`):

```cpp
struct frame_dimensions
{
    uint32_t scene_width{0};         // Dimensions for 3D offscreen targets (HDR, Depth, SSAO, Tonemap)
    uint32_t scene_height{0};
    uint32_t presentation_width{0};   // Dimensions for final UI / swapchain attachment
    uint32_t presentation_height{0};
};

void prepare_frame(const frame_dimensions& dims,
                   optional<rhi::texture_handle> swapchain_tex = nullopt,
                   optional<rhi::texture_view_handle> swapchain_view = nullopt,
                   optional<render_camera> camera_override = nullopt,
                   ui_render_callback ui_callback = nullptr);
```

### 2. Render Graph Target Sizing

- Surface-relative targets (`rg_texture_size::surface_relative(...)`) are resolved against `scene_width` and `scene_height` during `_graph.compile()` and `_graph.get_allocator().allocate()`.
- The final `UIRenderPass` and swapchain attachments are resolved against `presentation_width` and `presentation_height`.

### 3. Editor Viewport Integration

In `editor_engine_context._render_editor_frame()`:
- `viewport_window::draw()` tracks `_viewport_size` based on `ImGui::GetContentRegionAvail()`.
- When invoking `_renderer->render_frame()`, the editor passes `_viewport_size` as the scene render size, while the window manager provides the swapchain size:

```cpp
auto dims = render_system::frame_dimensions{
    .scene_width = _viewport_size.x > 0 ? _viewport_size.x : win_w,
    .scene_height = _viewport_size.y > 0 ? _viewport_size.y : win_h,
    .presentation_width = win_w,
    .presentation_height = win_h,
};
_renderer->render_frame(win.handle, dims, camera_override, ui_callback);
```

### 4. Rule #15 Compliance (Transient Allocator Eviction)
When the editor viewport window is resized (e.g. dragging docking splitters), transient render targets with mismatched dimensions are marked unused and evicted on their next idle flight cycle, guaranteeing no unbounded VRAM accumulation.
