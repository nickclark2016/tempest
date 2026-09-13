# Concurrency & Runtime Architecture Guidelines

This document provides deep-dive specifications, threading rules, and memory lifetime invariants for Tempest's concurrent runtime, job system, and dynamic libraries.

---

## 1. Strict Prohibition of `thread_local` and Global Variables
* **No TLS or Static Mutable State**: Never introduce `thread_local`, global variables, or static mutable state anywhere in engine runtime libraries.
* **Explicit Subsystem Instantiation**: Engine subsystems, allocators, profilers, and contexts must be explicitly instantiated and passed via references, non-owning pointers with well-defined lifetimes, or deduced through coroutine arguments and execution contexts.
* **Worker-Confinement**: Thread-confinement must be achieved through worker-indexed state structures (e.g. `worker_state` looked up via `find_current_worker()` or passed down through `job_context`) rather than TLS.
* **RAII Lifecycle**: Sockets, memory pools, allocators, and singletons must maintain clear RAII lifecycles tied to engine or system instances.

---

## 2. Dynamic Shared Library Lifetime & Destruction Order
When dynamically loading shared libraries (`shared_library::load`) that register callbacks, event listeners, polymorphic objects, or ECS components into engine subsystems:
* **Nested Lifetime Scope**: Always ensure the engine context, UI context, and registries are destructed **before** the shared library handles unload.
* **Explicit Block Scoping**: Encapsulate the engine context and its subsystems in an explicit nested scope (`{ ... }`) within the entrypoint before the shared library handles fall out of scope.
* **Dangling Pointers / Vtables**: Never unload a dynamic library while function pointers, lambdas, or vtables originating from that library remain active or registered in engine collections.

---

## 3. ThreadSanitizer (TSan) Verification Workflow
Whenever modifying the job system, thread pool, work-stealing queues, coroutine tasks/awaiters, synchronization primitives (`async_mutex`, `async_event`), or any multi-threaded runtime subsystem:
* **Mandatory Verification**: You must compile and run the relevant test suites with ThreadSanitizer enabled.
* **Premake Flag Requirement**: TSan flags are not enabled by default in Ninja build files. Premake must be explicitly invoked with `--use-tsan`:
  ```bash
  premake5 ninja --cc=clang --shared-engine --shell=posix --rhi-vulkan --use-tsan
  ```
* **Supported Test Targets**: Only non-GPU test targets (tagged `non-gpu-test` in Premake) support TSan:
  - `job-tests`
  - `profiler-tests`
  - `render-graph-tests`
  - `core-tests`
  - `ecs-tests`
  - `event-tests`
  - `serialization-tests`
  - `assets-tests`
* **GPU Target Restriction**: Do **not** run GPU-bound hardware driver tests (e.g., `rhi-vk-tests`) under TSan, as Vulkan ICD loader/driver memory layouts conflict with TSan shadow memory.
