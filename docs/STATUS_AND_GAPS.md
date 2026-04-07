# STATUS AND GAPS — fix bug v7

## Verified in container
- core build;
- CLI build;
- plugin build;
- automated tests via `ctest`;
- AddressSanitizer + UBSan test run;
- rejection of formula layers with no finite samples;
- rejection of non-finite numeric values from project files/import/manual point insertion;
- rejection of oversized and non-regular `.plotapp` files plus per-layer point/table limits;
- project save no longer uses a predictable `<project>.tmp` sidecar;
- managed desktop entry rendering now escapes substitutions and quotes launcher paths/text fields;
- shell passthrough disabled by default.

## Not fully verified in container
- full Qt6 desktop build and runtime behavior.

## Reason
- Qt6 development headers and libraries were not available in the container.

## Highest remaining validation targets on a real Qt6 machine
- `src/ui/MainWindow.cpp`
- `src/ui/PlotCanvasWidget.cpp`
- `src/ui/PointsEditorDialog.cpp`
- GNOME desktop install/update flow on a real workstation, especially when the user path contains spaces or special characters.

## Architectural note
- plugin loading is still a trusted-code boundary (`src/core/PluginManager.cpp`); this pass hardened validation around plugin results/params but did not redesign plugin isolation.
