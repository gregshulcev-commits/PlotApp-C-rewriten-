# Architecture

## High-level layout

```text
Qt6 UI (optional desktop frontend)
    |
    +-- PlotCanvasWidget
    +-- Layer list dock
    +-- Command console dock
    +-- Import / plugin / edit dialogs
    |
CommandDispatcher
    |
ProjectController
    |
    +-- Project model
    +-- Importers
    +-- PluginManager
    +-- ProjectSerializer
    +-- LayerSampler
    +-- SvgRenderer
```

## Layered design

### 1. Core model layer

The core owns the persistent truth:
- project settings
- layers
- visibility and styling
- formula metadata
- derived-layer provenance
- selected source-point indices for derived layers

The core remains UI-agnostic. The desktop selection UX is translated into model data rather than hidden in the widget layer.

### 2. Service layer

`ProjectController` coordinates workflows:
- importing a file into a new raw layer
- creating/regenerating formula layers
- applying a plugin to produce a derived layer
- normalizing an optional source-point subset before plugin execution
- saving/opening projects
- recomputing derived layers on project load
- exporting SVG

### 3. Extension layer

Plugins are loaded at runtime by `PluginManager` through a C ABI.
The ABI is defined in `include/plotapp/PluginApi.h`.

Approximation logic remains separate from the main application binary. The core now preserves plugin separation while still allowing selection-aware execution by passing a *subset view* of the chosen source layer into the plugin.

### 4. UI layer

The Qt UI is a thin shell above the controller.
It adds:
- a real plot window
- dialogs for file import and plugin execution
- a unified image export dialog with preview and format selection
- layer visibility toggles
- rectangular point selection for the currently selected layer
- `Esc`-driven selection clearing across the tree/canvas workflow
- an integrated command console
- a settings dialog that can surface managed-install version/update metadata

## Plugin targeting model

Plugins are intentionally **single-layer** operations.

```text
Current layer tree selection
 -> one source layer id
 -> optional rectangular point subset from PlotCanvasWidget
 -> ProjectController normalizes source indices
 -> PluginManager receives one source-layer view containing only selected points
 -> derived layer stores source layer id + selected source-point indices
```

This satisfies two important constraints:
1. a plugin cannot be accidentally applied to multiple layers at once;
2. reopening/recomputing a derived layer uses the same source subset instead of silently falling back to the entire original layer.

The desktop layer selection can also be cleared explicitly with `Esc`, which resets both the selected tree item and the selected source-point subset.

## Role metadata scope

`pointRoles` still exist in the model, but their meaning is now explicitly scoped to the `local_extrema` plugin.

- raw layers do not expose a `Role` editor in the UI;
- renderers only use role-based coloring when the layer supports extrema roles;
- this keeps generic layers free from min/max-specific semantics.

## Project file strategy

The project file stores:
- raw layer points
- derived layer points
- source layer id
- plugin id
- plugin params
- selected source-point indices for derived layers
- style and visibility

This provides three benefits:
1. a project can reopen even if a plugin is missing;
2. the app can optionally recompute derived layers later when the plugin is available again;
3. recomputation remains faithful to the original selection instead of expanding to the full source layer.

## Import pipeline

```text
File path
 -> importer selection by extension
 -> tabular preview
 -> user column selection
 -> default layer naming (`Y vs X`)
 -> numeric extraction
 -> raw layer creation
```

## Formula pipeline

```text
Expression + X range + sample count
 -> validation
 -> default layer naming from the expression
 -> stored formula metadata
 -> sampled points for persistence
 -> viewport-aware re-sampling in the renderer
```

The desktop formula dialog now seeds the default X range from the current viewport when data already exists, and adding a formula layer no longer forcibly resets the viewport for an existing project.

## Export pipeline

```text
Current project + current viewport
 -> Export dialog
 -> format selection (PNG/SVG)
 -> size preset selection (Current/A4 portrait/A4 landscape/Custom)
 -> preview rendering from the canvas composition
 -> file write via PNG raster export or SvgRenderer
```

This keeps raster and vector export in a single user workflow while still delegating final SVG generation to the headless renderer.

## Rendering split

- `SvgRenderer` is the headless renderer used in tests and automation.
- `PlotCanvasWidget` is the interactive window renderer used in the Qt UI.
- `LayerSampler` provides viewport-aware sampling for formulas and continuous derived layers.

This split keeps export and automated checks independent from GUI availability while allowing both renderers to respect stored provenance such as selected source subsets and error-bar bounds.

## Desktop update integration

The Qt settings dialog does **not** implement a second update engine.
Instead it:
- reads managed-install metadata from `installation.manifest` via shared core parsing utilities;
- shows build version plus installed version / install time / installed commit / repo / branch;
- invokes the stable managed-install shell workflow with `QProcess`;
- keeps the shell desktop manager as the single source of truth for GitHub/Git updates.

## Trusted plugin boundary

Plugin discovery still loads shared libraries from configured plugin directories. That remains a trusted-code boundary. This revision improved provenance and selection handling, but it did **not** redesign plugin sandboxing or isolation.
