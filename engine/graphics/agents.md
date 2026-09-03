# Modern Graphics

Modern graphics implementation for `GeneralsMD`.

```text
engine/graphics/
├── scene/
│   ├── views/
│   ├── visibility/
│   ├── lod/
│   ├── lighting/
│   ├── environment/
│   ├── shadows/
│   ├── decals/
│   ├── particles/
│   ├── transparency/
│   ├── gpu/
│   └── draw/
├── resources/
│   ├── handles/
│   ├── pools/
│   ├── meshes/
│   ├── textures/
│   ├── samplers/
│   ├── materials/
│   ├── residency/
│   └── bindless/
├── materials/
├── render-graph/
├── passes/
│   ├── opaque/
│   ├── shadow/
│   ├── transparent/
│   ├── decal/
│   ├── particles/
│   ├── sky/
│   └── post-process/
├── shaders/
├── rhi/
│   └── backends/
│       └── dx11/
├── memory/
└── graphics.{h,cpp}
```

Defaults:

- Handles/IDs, not object pointers.
- Bindless-style resource indexing.
- Batched/SoA data.
- Frame/linear allocators.
- Multithread-friendly extraction.
- Minimal state changes.
- GPU-driven where possible.
- Render graph owns synchronization and lifetimes.
- RHI stays thin.

Organization:

- Divide responsibilities into lean subfolders under `engine/graphics`.
- Keep resource responsibilities in dedicated subfolders such as `handles`, `pools`, `meshes`, `textures`, `samplers`, and `materials`.
- Keep bindless resource indexing in `resources/bindless`.
- Keep implementation modules and their tests inside the relevant responsibility subfolder, not directly in the graphics root.
- Place one colocated Boost.Test module (`*.test.cppm`) beside each implementation module.
- Test public interfaces only.

Modules:

- Use modern C++20 modules (`.cppm`) for code under `engine/graphics`.
- This modules requirement applies only to `engine/graphics`.
- Use `Graphics` as the module namespace; do not prefix graphics modules with `Generals`.
- Do not add comments to code under `engine/graphics`, including boilerplate file headers.
- Use Boost.Test for colocated tests.

Coordinate convention:

- Preserve Generals/WW3D2: right-handed, +Z up, -Z camera forward, upper-left viewport origin; convert per backend below `View`.

Hard constraints:

- Prioritize predictable, high-performance engine code over overly defensive general-purpose C++.
- Do not use exceptions for renderer control flow or transactional rollback.
- Use `noexcept` for hot-path and foundational renderer operations where practical.
- Avoid hidden allocations in per-frame paths.
- Prefer validate/prepare/commit mutation patterns.
- Dense resource types should be cheap and `noexcept` movable.
- Keep backend/API-specific concepts inside `rhi`.
- Place each backend under `rhi/backends/<backend>`.
- Use stable typed handles rather than persistent pointers into movable storage.
- Keep foundational containers simple and deterministic.
- If exception safety requires substantial bookkeeping or rollback logic, stop and reconsider the design instead of adding complexity.
- Before completing a graphics change, review it against these constraints and call out any deliberate exception.

Layout:

- Hot scene, bounds, light, and particle data use aligned contiguous SoA columns.
- GPU upload rows and compact LOD/draw submission records remain aligned AoS where consumers read complete records.
