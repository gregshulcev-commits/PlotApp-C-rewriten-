# PlotApp Modular UI

PlotApp is a modular plotting application built around a C++17 core, a CLI/command-console workflow, and a Qt6 desktop UI wrapper.

This revision keeps the layered architecture intact and focuses on plugin-targeted selection, formula-UX fixes, and safer persistence of derived-layer provenance.

## What changed in this update

- `Role` is now treated as plugin-specific metadata instead of a global point attribute:
  - the point editor only exposes **Role** for the `local_extrema` / min-max layer;
  - legends and point coloring only interpret `pointRoles` for that plugin.
- fixed the common `exp(x)` workflow problem in the UI:
  - when a formula layer is added to a project that already has visible data, the formula dialog is seeded from the **current X viewport** instead of always defaulting to `[-10, 10]`;
  - adding a formula layer no longer forcibly resets the viewport for an existing project, so very large functions such as `exp(x)` do not immediately collapse the rest of the plot.
- fixed formula parser precedence so `-x^2` is interpreted as `-(x^2)` instead of `(-x)^2`.
- fixed plot bounds for error bars in both the interactive canvas and SVG export.
- added single-layer, selection-aware plugin execution in the desktop workflow:
  - selecting a layer in the layer tree selects that one layer as the only plugin target;
  - clicking a layer selects its full point set (or the whole formula layer);
  - `Shift + left drag` on the plot performs rectangular point selection **only for the currently selected layer**;
  - applying a plugin now uses the selected subset of source points instead of silently using the full layer.
- derived-layer provenance now persists the selected source-point indices in the project file, so saving/reopening/recomputing keeps the same plugin input subset.
- continuous derived-layer viewport sampling now honors the stored source subset, which keeps recomputed fits/splines consistent with the original plugin run.
- project save/load format was extended to store derived-layer source selections while remaining backward-compatible with older project files.

## Build

### Core + CLI only
```bash
cmake -S . -B build -G Ninja -DPLOTAPP_BUILD_GUI=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

### Full desktop UI on Fedora
```bash
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel minizip-ng-compat-devel
cmake -S . -B build -G Ninja
cmake --build build
```

## Run

Shell passthrough from the command console is now disabled by default. Enable it explicitly with `PLOTAPP_ENABLE_SHELL=1` only when you really need it.


CLI:
```bash
./build/plotapp-cli help
./build/plotapp-cli "import ./examples/sample_points.csv 0 1 raw"
./build/plotapp-cli "formula x^2 -2 2 parabola"
```

GUI:
```bash
./build/plotapp
```

## UI controls
- Mouse drag on plot area: pan viewport
- Mouse wheel: zoom both axes
- `Shift + wheel`: X-axis zoom only
- `Ctrl + wheel`: Y-axis zoom only
- `Shift + left drag` on the plot: rectangular point selection for the currently selected layer
- Drag legend box: move that layer's legend
- Click title / X label / Y label: edit text inline
- Click a layer in the layer tree: select that one layer as the current plugin target and select all of its points
- `Ctrl + S`: save project

## Formula syntax
Examples:
- `sin(x)`
- `sin(x) + 0.2*x^2`
- `exp(-x^2)`
- `sqrt(abs(x))`
- `1e-3*x`

Supported:
- variable: `x`
- operators: `+ - * / ^`
- constants: `pi`, `e`
- functions: `sin cos tan asin acos atan sqrt abs exp log ln log10 floor ceil`

Notes:
- exponentiation binds tighter than unary minus, so `-x^2` means `-(x^2)`;
- the formula-layer dialog uses the current visible X-range as the default source range when a project already contains data;
- adding a formula layer to an existing project keeps the current viewport instead of automatically resetting to the new formula bounds.

## LaTeX-like labels
The Qt canvas supports a lightweight LaTeX-like subset for title, axis labels and legend text:
- subscripts: `I_1`, `I_{out}`
- superscripts: `x^2`, `E^{max}`
- some Greek symbols: `\alpha`, `\beta`, `\mu`, `\sigma`, `\Delta`, `\Omega`, `\pi`


## Managed install, update and removal

PlotApp now includes a shell-first managed desktop install flow for Linux.

Install from the current source tree:
```bash
./install_app.sh --with-gui
```

Bind or inspect GitHub updates:
```bash
./update_app.sh --set-repo https://github.com/example/plotapp.git --branch main
./update_app.sh --check-only
```

Update to the newest revision:
```bash
./update_app.sh --yes
```

Remove the managed payload but keep runtime data:
```bash
./uninstall_app.sh --yes
```

Remove everything including runtime data/cache:
```bash
./uninstall_app.sh --purge-data --yes
```

The managed installer uses:
- `~/.local/share/plotapp-install/app/current` for the active payload,
- `~/.local/share/plotapp-install/app/previous` for rollback,
- `~/.local/share/plotapp-install/metadata/installation.manifest` as the shell-readable source of truth,
- `~/.local/share/plotapp-install/metadata/installation.json` as the diagnostic JSON mirror,
- `~/.local/bin/plotapp*` plus a GNOME desktop entry/icon for user-facing integration.

See `docs/MANAGED_INSTALL.md` for the full layout, manifest format, update workflow, release discipline, and GUI integration notes for the new Updates tab.

When PlotApp is launched from a managed installation, the same workflow is also available in **File -> Settings -> Updates**.

## Status note
The core library, plugins, CLI, serializer changes, and automated tests were rebuilt and verified in the container.
The desktop UI sources were updated for layer-bound selection, formula-range seeding, and point-role scoping. The container build in this session did **not** emit a runnable Qt desktop binary, so the new desktop interactions should still be smoke-tested on a real Qt workstation build.
