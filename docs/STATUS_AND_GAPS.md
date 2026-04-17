# STATUS AND GAPS

## Verified in container
- core build
- plugin build
- CLI build
- automated tests via `ctest`
- formula parser precedence fix (`-x^2`)
- selection-aware derived-layer provenance in project save/load/recompute
- continuous derived-layer viewport sampling from the stored source subset
- error-bar bounds in the headless SVG pipeline
- project roundtrip with persisted selected source-point indices

## Implemented in source but not fully verified in a real desktop session
- full Qt desktop runtime behavior for the new selection workflow
- interactive rectangular point selection UX in `PlotCanvasWidget`
- layer-tree click/reclick behavior for whole-layer reselection
- formula dialog seeding from the current viewport inside a live Qt session
- point-editor visibility of the `Role` column only for extrema layers

## Reason
The container session rebuilt and tested the non-Qt targets successfully, but it did not emit a runnable Qt desktop binary for interactive smoke-testing.
The desktop source files were updated, however the new UX should still be checked on a real workstation build.

## Highest remaining validation targets on a Qt workstation
- `src/ui/MainWindow.cpp`
- `src/ui/PlotCanvasWidget.cpp`
- `src/ui/PointsEditorDialog.cpp`
- `src/ui/FormulaLayerDialog.cpp`

## Architectural note
- plugin loading remains a trusted-code boundary (`src/core/PluginManager.cpp`)
- selection support was added without changing the plugin ABI; the core now filters the source-layer view before calling the plugin
- derived layers now persist the selected source-point indices needed to recompute them faithfully
