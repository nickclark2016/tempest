# Architecture, ECS & Asset Pipeline Guidelines

This document provides deep-dive specifications, post-mortem rationales, and architectural constraints for ECS hierarchy operations, asset databases, and binary chunk serialization.

---

## 1. ECS Hierarchy Traversal for Scene & Prefab Loading
When ingesting, instantiating, or uploading entity hierarchies (such as glTF models or composite prefabs) to GPU memory:
* **No Root Assumption**: Do not assume renderable components (`mesh_component`, `material_component`, `renderable_component`) reside on root entities.
* **Recursive Hierarchy Traversal**: Always recursively traverse `ecs::relationship_component<ecs::entity>` (`first_child`, `next_sibling`) to discover and upload all child entities, submeshes, and material references.

---

## 2. Binary Chunk Arena & Asset Packing Isolation
In binary asset databases, serialization chunk arenas, or pooled memory streams:
* **Dedicated Cursor Isolation**: Never share packing cursor state (`current_chunk_used`, `current_chunk_capacity`) across dedicated large-asset allocations and small-asset packing buffers.
* **Arena Isolation**: Always isolate small-asset packing into dedicated arena buffers so appending large standalone chunks cannot corrupt packing offsets or cause small object payloads to overwrite large asset buffers in memory.

---

## 3. Subsystem Ownership Patterns
* **Parent-Child Hierarchy**: When parent systems own child subsystems, prefer unique ownership (`tempest::unique_ptr`) on the parent, exposing child references (`T&`) or non-owning pointers (`T*` / `non_null<T>`) downstream.
* **Generational Indices**: For dynamic world objects and scene resources, represent identity via generational indices (`ecs::entity` validated against `registry.is_valid(entity)`).
* **Deterministic Teardown**: Subsystems must be torn down in the exact reverse order of creation. Shared ownership (`shared_ptr`) is prohibited because it masks teardown order and introduces non-deterministic destructor execution.
