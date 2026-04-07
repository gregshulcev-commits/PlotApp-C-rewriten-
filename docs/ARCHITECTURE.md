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
    +-- SvgRenderer
```

## Layered design

### 1. Core model layer

The core owns the persistent truth:
- project settings
- layers
- visibility
- styles
- generated provenance

The core is intentionally UI-agnostic.

### 2. Service layer

`ProjectController` coordinates workflows:
- importing a file into a new raw layer
- applying a plugin to produce a derived layer
- saving/opening projects
- recomputing derived layers on project load
- exporting SVG

### 3. Extension layer

Plugins are loaded at runtime by `PluginManager` through a C ABI.
The ABI is defined in `include/plotapp/PluginApi.h`.

This allows the approximation code to remain separate from the main application.

### 4. UI layer

The Qt UI is a thin shell above the controller.
It adds:
- a real plot window
- dialogs for file import and plugin execution
- layer visibility toggles
- an integrated command console
- a settings dialog that can surface managed-install version/update metadata

## Why this design matches the request

The request explicitly asked for a modular structure where approximation logic behaves like a separate program or library that gets access to a layer and returns a result.
That is exactly what the plugin ABI implements.

The request also asked for a UI that can still preserve a simpler command-line mode.
That is why the UI includes a command console backed by the same `CommandDispatcher` used by the CLI executable.

## Project file strategy

The project file stores:
- raw layer points
- derived layer points
- source layer id
- plugin id
- plugin params
- style and visibility

This gives two benefits:
1. the project can reopen even if a plugin is missing
2. the app can optionally recompute derived layers later when the plugin is available again

## Import pipeline

```text
File path
 -> importer selection by extension
 -> tabular preview
 -> user column selection
 -> numeric extraction
 -> raw layer creation
```

## Plugin pipeline

```text
Selected source layer
 -> runtime plugin lookup
 -> plugin request with source points + params
 -> generated result points
 -> new derived layer
```

## Rendering split

- `SvgRenderer` is the headless renderer used in tests and automation.
- `PlotCanvasWidget` is the interactive window renderer used in the Qt UI.

This split keeps export and automated checks independent from GUI availability.

## Desktop update integration

The Qt settings dialog does **not** implement a second update engine.
Instead it:
- reads managed-install metadata from `installation.manifest` via shared core parsing utilities;
- shows build version plus installed version / install time / installed commit / repo / branch;
- invokes the stable managed-install shell workflow with `QProcess`;
- keeps the shell desktop manager as the single source of truth for GitHub/Git updates.
