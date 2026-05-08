## ADR-001: Assimp Model Import Adapter
**Status**: Accepted

**Context**: The engine needs model loading with an API comparable to texture loading, while keeping third-party types out of core rendering and preserving asynchronous asset loading behavior. Assimp is distributed here as prebuilt runtime binaries for Windows, Linux, and macOS, so the build must select the correct runtime per platform.

**Decision**: Add a core model asset contract and an `IModelImporter` abstraction, with Assimp isolated behind a platform adapter. The adapter loads the Assimp C API dynamically from the selected runtime binary and converts imported meshes into engine-owned `StaticMeshVertex` and index buffers. `RenderSystem` exposes synchronous and asynchronous model loading APIs; asynchronous loading performs model import on a background thread and uploads meshes to the render backend from the foreground render flow.

**Alternatives considered**: Linking directly against Assimp import libraries was rejected because the packaged dependency currently ships runtime binaries without a consistent import-library layout across platforms. Exposing Assimp scene or mesh types from core was rejected because it would leak third-party infrastructure into the engine API. Uploading GPU resources from the worker thread was rejected because render backend ownership and synchronization must stay centralized.

**Consequences**: Model loading can now evolve behind the importer interface, and platform-specific runtime selection stays in CMake. The first implementation imports static mesh geometry only; materials, skeletons, animations, and scene hierarchy can be added later without changing callers that only need mesh handles.
