# Coordinate Systems & Perspective Projection

This document outlines the coordinate system conventions, transformation pipelines, and the mathematical derivation of the camera projection matrices used in the Tempest Engine.

---

## 1. Overview & Space Pipeline

Tempest follows a standard right-handed 3D coordinate system for world and view spaces, transformed into Vulkan clip and Normalized Device Coordinate (NDC) spaces for rasterization:

```
World Space (Right-Handed, +Y Up, +Z Forward by default)
       │
       ▼  [View Matrix: look_at]
View Space (Right-Handed, +X Right, +Y Up, -Z Forward / View Direction)
       │
       ▼  [Projection Matrix: perspective]
Clip Space (4D Homogeneous: [x_c, y_c, z_c, w_c])
       │
       ▼  [Perspective Division: / w_c]
Normalized Device Coordinates (NDC) (Vulkan: [-1, 1] X, [-1, 1] Y (Down), [0, 1] Z)
       │
       ▼  [Viewport Transform: set_viewport]
Screen / Framebuffer Space (Origin at Top-Left: [0, 0] to [Width, Height])
```

---

## 2. Coordinate Spaces & Conventions

### 2.1 World & Transform Conventions
* **Position**: Standard Cartesian `(x, y, z)`.
* **Orientation**: Represented by unit quaternions `tempest::math::quat<T>`.
* **Standard Directions** (from identity orientation):
  * **Forward**: $+Z = (0, 0, 1)$ via `extract_forward(rotation)`
  * **Up**: $+Y = (0, 1, 0)$ via `extract_up(rotation)`
  * **Right**: $+X = (1, 0, 0)$ via `extract_right(rotation)`

### 2.2 View Space
The view matrix is generated via `tempest::math::look_at(eye, target, up)` in [`transformations.hpp`](../../engine/runtime/math/include/tempest/transformations.hpp).
* The camera is located at `eye` and looks toward `target = eye + forward`.
* **Right-Handed View Space**:
  * $+X_{\text{view}}$ points to the **right** of the camera.
  * $+Y_{\text{view}}$ points **upwards** relative to the camera.
  * $-Z_{\text{view}}$ points along the **view direction** (in front of the camera).

For any point $\vec{p}$ located at distance $d > 0$ along the camera's forward direction:
$$z_{\text{view}} = -d < 0$$

### 2.3 Vulkan Normalized Device Coordinates (NDC)
Vulkan NDC defines clip space bounds where coordinates outside the following volumes are clipped:
$$-1 \le x_{\text{ndc}} \le 1, \quad -1 \le y_{\text{ndc}} \le 1, \quad 0 \le z_{\text{ndc}} \le 1$$

Crucial differences from legacy graphics APIs (such as OpenGL):
| Property | Vulkan | OpenGL | Direct3D |
| :--- | :--- | :--- | :--- |
| **$Y$-axis direction** | **Downwards** ($+1$ is bottom, $-1$ is top) | **Upwards** ($+1$ is top, $-1$ is bottom) | **Upwards** ($+1$ is top, $-1$ is bottom) |
| **$Z$-axis depth range** | **$[0, 1]$** ($0$ near, $1$ far standard) | **$[-1, 1]$** | **$[0, 1]$** |
| **Framebuffer origin** | **Top-Left** $(0, 0)$ | **Bottom-Left** $(0, 0)$ | **Top-Left** $(0, 0)$ |

Because Vulkan's viewport transformation maps $y_{\text{ndc}} = -1.0$ to the **top** of the screen and $y_{\text{ndc}} = +1.0$ to the **bottom**, an object above the camera ($y_{\text{view}} > 0$) must project to a negative NDC coordinate ($y_{\text{ndc}} < 0$).

---

## 3. Infinite Reverse-$Z$ Perspective Projection

Tempest uses an **Infinite Reverse-$Z$ Perspective Projection** matrix as its default camera projection in [`render_system::camera_system`](../../engine/runtime/render/system/src/camera_system.cpp).

### 3.1 Mathematical Derivation

Let:
* $\text{fov}_y$ be the vertical field of view in radians.
* $\text{aspect} = \frac{\text{width}}{\text{height}}$ be the aspect ratio.
* $f = \cot\left(\frac{\text{fov}_y}{2}\right) = \frac{1}{\tan(\text{fov}_y / 2)}$ be the focal length.
* $\text{near}$ be the distance to the near clipping plane ($\text{near} > 0$).
* A view-space point be $\vec{v}_{\text{view}} = (x_v, y_v, z_v, 1)^T$, where $z_v = -d$ with $d > 0$.

We seek a $4 \times 4$ projection matrix $P$ such that $\vec{v}_{\text{clip}} = P \cdot \vec{v}_{\text{view}}$ and $\vec{v}_{\text{ndc}} = \vec{v}_{\text{clip}} / w_{\text{clip}}$.

#### 1. $W_{\text{clip}}$ Component (Perspective Division)
We require perspective division by the distance $d = -z_v$:
$$w_{\text{clip}} = -z_v$$
Therefore, row 3 of $P$ is:
$$\text{Row}_3 = \begin{pmatrix} 0 & 0 & -1 & 0 \end{pmatrix}$$

#### 2. $X_{\text{clip}}$ Component (Horizontal FOV)
$$x_{\text{clip}} = \frac{f}{\text{aspect}} x_v \implies x_{\text{ndc}} = \frac{f \cdot x_v}{\text{aspect} \cdot (-z_v)} = \frac{f \cdot x_v}{\text{aspect} \cdot d}$$
Points to the right of the camera ($x_v > 0$) map to $x_{\text{ndc}} > 0$ (right side of viewport).
$$\text{Row}_0 = \begin{pmatrix} \frac{f}{\text{aspect}} & 0 & 0 & 0 \end{pmatrix}$$

#### 3. $Y_{\text{clip}}$ Component ($Y$-Inversion for Vulkan)
In view space, points above the camera have $y_v > 0$. In Vulkan NDC, the top of the viewport has $y_{\text{ndc}} < 0$.
To map $y_v > 0 \implies y_{\text{ndc}} < 0$:
$$y_{\text{clip}} = -f \cdot y_v \implies y_{\text{ndc}} = \frac{-f \cdot y_v}{-z_v} = -\frac{f \cdot y_v}{d}$$
Using $-f$ ensures geometry is right-side up in Vulkan without requiring negative viewport heights.
$$\text{Row}_1 = \begin{pmatrix} 0 & -f & 0 & 0 \end{pmatrix}$$

#### 4. $Z_{\text{clip}}$ Component (Reverse-$Z$ with Infinite Far Plane)
For Reverse-$Z$, we want:
* At the near plane ($d = \text{near}$ / $z_v = -\text{near}$): $z_{\text{ndc}} = 1.0$
* At infinity ($d \to \infty$ / $z_v \to -\infty$): $z_{\text{ndc}} = 0.0$

Let $z_{\text{clip}} = A z_v + B$. Then:
$$z_{\text{ndc}} = \frac{A z_v + B}{-z_v} = -A + \frac{B}{-z_v} = -A + \frac{B}{d}$$

Solving the boundary conditions:
1. As $d \to \infty$:
   $$\lim_{d \to \infty} \left( -A + \frac{B}{d} \right) = -A = 0.0 \implies A = 0$$
2. At $d = \text{near}$:
   $$\frac{B}{\text{near}} = 1.0 \implies B = \text{near}$$

Thus:
$$z_{\text{clip}} = 0 \cdot z_v + \text{near} = \text{near}$$
$$\text{Row}_2 = \begin{pmatrix} 0 & 0 & 0 & \text{near} \end{pmatrix}$$

---

### 3.2 Full Matrix Formulation

Combining rows 0 through 3 yields the mathematical matrix $P$:

$$
P = \begin{pmatrix}
\frac{f}{\text{aspect}} & 0 & 0 & 0 \\
0 & -f & 0 & 0 \\
0 & 0 & 0 & \text{near} \\
0 & 0 & -1 & 0
\end{pmatrix}
$$

---

## 4. Implementation Details in Engine Code

### 4.1 Memory Layout & Constructor Storage
In [`tempest::math::mat4`](../../engine/runtime/math/include/tempest/mat4.hpp), matrices are stored in **column-major** order. The 16-parameter constructor accepts arguments grouped by column:

$$\text{mat4}(c_{00}, c_{10}, c_{20}, c_{30}, \quad c_{01}, c_{11}, c_{21}, c_{31}, \quad c_{02}, c_{12}, c_{22}, c_{32}, \quad c_{03}, c_{13}, c_{23}, c_{33})$$

Where $c_{ij}$ is Row $i$, Column $j$.

Extracting the columns of $P$:
* **Column 0**: $\begin{pmatrix} f / \text{aspect} & 0 & 0 & 0 \end{pmatrix}^T$
* **Column 1**: $\begin{pmatrix} 0 & -f & 0 & 0 \end{pmatrix}^T$
* **Column 2**: $\begin{pmatrix} 0 & 0 & 0 & -1 \end{pmatrix}^T$
* **Column 3**: $\begin{pmatrix} 0 & 0 & \text{near} & 0 \end{pmatrix}^T$

In [`transformations.hpp`](../../engine/runtime/math/include/tempest/transformations.hpp):
```cpp
template <typename T>
inline constexpr mat4<T> perspective(const T aspect, const T fov, T near)
{
    const T fovy = fov;
    const T f = static_cast<T>(1) / std::tan(fovy / 2);
    return mat4<T>{
        f / aspect, 0,  0, 0,     // Column 0
        0,         -f,  0, 0,     // Column 1
        0,          0,  0, -1,    // Column 2
        0,          0,  near, 0   // Column 3
    };
}
```

### 4.2 Shaders (Slang)
In shaders (e.g. [`zprepass.slang`](../../engine/runtime/render/system/shaders/raster/zprepass.slang), [`pbr.slang`](../../engine/runtime/render/system/shaders/raster/pbr.slang)), clip positions are computed via standard column-vector multiplication:
```slang
float4 world_pos = mul(instance.model_matrix, float4(v.position, 1.0));
float4 view_pos  = mul(scene.camera.view, world_pos);
float4 clip_pos  = mul(scene.camera.projection, view_pos);
```

---

## 5. Reverse-$Z$ & Depth Testing Configuration

### 5.1 Why Reverse-$Z$?
Floating-point numbers (`float32`) have non-linear precision: roughly half of all representable values lie between $[-1.0, 1.0]$, with exponential density approaching $0.0$.

* **Standard $Z$ ($[0 \to 1]$)**: Precision is highest near the camera ($z_{\text{ndc}} \approx 0.0$) and lowest at distance ($z_{\text{ndc}} \approx 1.0$), compounding the non-linear $1/z$ perspective distribution and causing severe $Z$-fighting on distant geometry.
* **Reverse-$Z$ ($[1 \to 0]$)**: The near plane maps to $1.0$ and distant geometry maps towards $0.0$. The floating-point exponent bits counteract the $1/z$ perspective non-linearity, providing virtually constant precision across vast world scales without requiring an explicit far plane.

### 5.2 Pipeline & Pass State
All rasterization passes using depth testing in Tempest configure:
* **Depth Clear Value**: `0.0f`
* **Depth Compare Operation**: `rhi::compare_op::greater` (or `greater_or_equal`)
* **Depth Format**: `rhi::data_format::depth32_float`

Example from [`depth_prepass.cpp`](../../engine/runtime/render/system/src/passes/depth_prepass.cpp):
```cpp
.depth_stencil_state = {
    .depth_test_enable = true,
    .depth_write_enable = true,
    .depth_compare_op = rhi::compare_op::greater,
},
// Attachment clear:
.clear_value = {.depth = 0.0F, .stencil = 0},
```

---

## 6. Summary Checklist & Guidelines

1. **Always use `-f` for $Y$-scaling** in projection matrices when targeting Vulkan without negative viewports.
2. **Clear depth buffers to `0.0f`** and use **`greater`** comparison operations when rendering with Reverse-$Z$.
3. Camera rotations define $+Z$ forward and $+Y$ up in world space; `look_at` transforms this into $-Z$ forward in view space.
