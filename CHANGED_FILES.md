# CHANGED FILES (UI / EXPORT / PLUGIN UX UPDATE)

## Added

- `src/ui/DialogUtil.h`
- `src/ui/ExportDialog.h`
- `src/ui/ExportDialog.cpp`
- `docs/PLUGIN_AUTHORING_RU.md`

## Modified

- `CMakeLists.txt`
- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/PLUGIN_API.md`
- `docs/REQUEST_TRACEABILITY.md`
- `docs/STATUS_AND_GAPS.md`
- `docs/USER_GUIDE.md`
- `docs/CHANGELOG_RU.md`
- `include/plotapp/ProjectController.h`
- `plugins/linear_fit/linear_fit_plugin.cpp`
- `src/core/ProjectController.cpp`
- `src/ui/FormulaLayerDialog.h`
- `src/ui/FormulaLayerDialog.cpp`
- `src/ui/ImportDialog.h`
- `src/ui/ImportDialog.cpp`
- `src/ui/LayerPropertiesDialog.cpp`
- `src/ui/MainWindow.h`
- `src/ui/MainWindow.cpp`
- `src/ui/PlotCanvasWidget.h`
- `src/ui/PlotCanvasWidget.cpp`
- `src/ui/PluginRunDialog.h`
- `src/ui/PluginRunDialog.cpp`
- `src/ui/PointsEditorDialog.cpp`
- `src/ui/SettingsDialog.cpp`
- `src/ui/TextEntryDialog.cpp`
- `tests/tests_main.cpp`

## Main theme of the revision

- add `Esc`-based clearing of the active layer/point selection;
- default imported-layer names and legends to `Y vs X` and formula-layer names/legends to the formula expression;
- replace separate PNG/SVG desktop export actions with one export dialog that supports PNG/SVG selection, A4 presets, DPI, and preview;
- add a plugin-specific `linear_fit` option for showing axis intersections;
- enlarge desktop dialogs to medium default sizes;
- expand the documentation, including a detailed Russian plugin-authoring guide.
