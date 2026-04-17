# REQUEST TRACEABILITY

This file maps the latest requested changes to the current source update.

## 1. Keep plugins separate from the core and make them apply to one layer only
- the runtime plugin ABI remains separate in `include/plotapp/PluginApi.h` and `src/core/PluginManager.cpp`
- the desktop workflow now uses single-layer targeting from the layer tree
- `ProjectController::applyPlugin(...)` was extended to accept an optional source-point subset while still calling the existing plugin ABI
- plugin-generated layers remain children of their source layer in the tree

## 2. `Role` should appear only for min/max / `local_extrema`
- `include/plotapp/Project.h` now scopes role interpretation through `layerSupportsPointRoles(...)`
- `src/ui/PointsEditorDialog.cpp` hides the `Role` column for unrelated layers
- `src/ui/PlotCanvasWidget.cpp` and `src/render/SvgRenderer.cpp` only use role-based secondary coloring for role-supporting layers

## 3. Fix the `exp(x)` scale-collapse workflow bug
- `src/ui/FormulaLayerDialog.*` now accepts a suggested X range
- `src/ui/MainWindow.cpp` seeds the formula dialog from the current viewport X range
- adding a formula layer to an existing project no longer forces `resetViewToProject()`, which prevents immediate collapse of the rest of the plot when the new formula has a very large Y range

## 4. Support selecting part of a graph and applying a plugin only to the selected points of the selected layer
- `src/ui/PlotCanvasWidget.*` adds layer-bound selection state and rectangular selection (`Shift + left drag`)
- selecting a layer in the tree selects the whole layer by default
- `src/ui/MainWindow.cpp` passes the current canvas selection into plugin execution
- `include/plotapp/Project.h` adds `pluginSourcePointIndices`
- `src/core/ProjectController.cpp` normalizes, stores, and reuses selected source indices
- `src/core/LayerSampler.cpp` now re-samples continuous derived layers from the stored subset instead of the entire source layer
- `src/serialization/ProjectSerializer.cpp` persists selected source-point indices in the project format

## 5. Fix additional correctness bugs found during the audit
- `src/core/FormulaEvaluator.cpp` fixes precedence for unary minus vs exponentiation (`-x^2`)
- `src/ui/PlotCanvasWidget.cpp` and `src/render/SvgRenderer.cpp` include error-bar height in bounds calculations

## 6. Update documentation for the new behavior
- updated: `README.md`
- updated: `docs/USER_GUIDE.md`
- updated: `docs/ARCHITECTURE.md`
- updated: `docs/PLUGIN_API.md`
- updated: `docs/STATUS_AND_GAPS.md`
- added: `docs/CHANGELOG_RU.md`
