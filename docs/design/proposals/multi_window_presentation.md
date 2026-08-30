# Proposal: Multi-Window Swapchain Lifecycle & Presentation Batching

## Status
Proposed

## Context
The engine supports multiple top-level and auxiliary OS windows (e.g. secondary viewports, asset inspectors, node editors). The `render_system::renderer` encapsulates a collection of `_surfaces` indexed by `window_handle`.

Currently, frame rendering processes windows sequentially via individual calls to `render_frame(win.handle)`. For advanced multi-window workflows, presenting across multiple windows concurrently benefits from batch queue synchronization and synchronized flight slot advancement.

## Proposed Design

### 1. Batch Multi-Surface Rendering API

Provide a batch interface on `renderer` to render multiple windows within a single frame cycle:

```cpp
struct window_render_target
{
    window_handle window;
    optional<render_camera> camera_override{nullopt};
    ui_render_callback ui_callback{nullptr};
};

auto render_surfaces(span<const window_render_target> targets)
    -> expected<void, render_graph::execution_error>;
```

### 2. Multi-Queue and Multi-Swapchain Synchronization

- **Acquisition**: `renderer` acquires next swapchain images across all active windows during `begin_frame()`.
- **DAG Execution**: Individual Render Graph passes can target separate surface attachments or share cached render resources (e.g., sharing shadow maps or scene buffers across multiple editor viewport windows).
- **Presentation**: `renderer::present_all()` presents swapchain images for all rendered surfaces sequentially or in batch to the graphics queue.
