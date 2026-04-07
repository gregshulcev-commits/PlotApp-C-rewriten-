# STATUS AND GAPS — fix bug v5

## Verified in container
- core build;
- CLI build;
- plugin build;
- automated tests via `ctest`;
- AddressSanitizer + UBSan test run;
- rejection of formula layers with no finite samples;
- rejection of non-finite numeric values from project files/import/manual point insertion;
- shell passthrough disabled by default.

## Not fully verified in container
- full Qt6 desktop build and runtime behavior.

## Reason
- Qt6 development headers and libraries were not available in the container.

## Highest remaining validation targets on a real Qt6 machine
- `src/ui/MainWindow.cpp`
- `src/ui/PlotCanvasWidget.cpp`
- `src/ui/PointsEditorDialog.cpp`

## Architectural note
- plugin loading is still a trusted-code boundary (`src/core/PluginManager.cpp`); this pass hardened validation around plugin results/params but did not redesign plugin isolation.
