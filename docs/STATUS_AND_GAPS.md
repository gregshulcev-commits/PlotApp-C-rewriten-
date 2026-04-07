# STATUS AND GAPS — fix bug v8

## Verified in container
- core build;
- CLI build;
- plugin build;
- automated tests via `ctest`;
- AddressSanitizer + UBSan test run;
- build-version embedding from `VERSION.txt` into the compiled core;
- managed-install manifest parsing and update-status parsing in the shared core layer;
- rejection of formula layers with no finite samples;
- rejection of non-finite numeric values from project files/import/manual point insertion;
- rejection of oversized and non-regular `.plotapp` files plus per-layer point/table limits;
- project save no longer uses a predictable `<project>.tmp` sidecar;
- managed desktop entry rendering now escapes substitutions and quotes launcher paths/text fields;
- shell passthrough disabled by default.

## Implemented in source but not fully verified in container
- full Qt6 desktop build and runtime behavior;
- the new **Settings -> Updates** tab;
- real GitHub network update flow through the GUI buttons.

## Reason
- Qt6 development headers and libraries were not available in the container;
- the container also does not provide a real desktop session or a real GitHub-connected workstation installation.

## Highest remaining validation targets on a real Qt6 machine
- `src/ui/SettingsDialog.cpp`
- `src/ui/MainWindow.cpp`
- `src/ui/PlotCanvasWidget.cpp`
- GNOME desktop install/update flow on a real workstation, including the GUI update tab and restart UX.

## Architectural note
- plugin loading is still a trusted-code boundary (`src/core/PluginManager.cpp`); this pass hardened validation around plugin results/params but did not redesign plugin isolation.
- the GUI updater intentionally reuses `desktop_manager.sh update` instead of introducing a second update engine.
