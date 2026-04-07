# PlotApp Modular UI

PlotApp is a modular plotting application built around a C++17 core, a CLI/command-console workflow, and a Qt6 desktop UI wrapper.

This revision keeps the layered architecture intact and focuses on bug fixing, security hardening, and a few high-value functional improvements in the core/CLI path.

## What changed in this update

- fixed broken automated tests caused by fragile relative paths;
- hardened plugin discovery so the app no longer auto-loads arbitrary `.so` files from the current working directory;
- added safer default plugin discovery relative to the executable, plus optional `PLOTAPP_PLUGIN_DIR` override;
- added protection against duplicate plugin IDs and repeated `discover()` handle leaks;
- improved delimited-text import:
  - extensionless tabular files now work,
  - delimiter detection is more robust,
  - UTF-8 BOM is handled,
  - simple decimal-comma numeric cells are accepted,
  - obvious binary/image-like files are rejected as tabular data;
- added XLSX entry-size limits to reduce ZIP-bomb style risk;
- hardened project save/load:
  - project header/version is validated,
  - oversized and non-regular project files are rejected before parsing,
  - per-project layer/point/table limits reduce `.plotapp` memory-exhaustion risk,
  - malformed numeric fields report clearer errors,
  - save now uses a unique adjacent temp file + atomic rename flow instead of a predictable `<project>.tmp` sidecar;
- fixed CLI `formula` parsing so a layer name can be passed without explicitly providing the sample count;
- formula layers that produce fewer than two finite points are now rejected instead of creating empty/broken layers;
- NaN/Inf numeric input is now rejected in project files, imports, plugin numeric params, and manual point insertion;
- improved SVG export:
  - title/labels/legend text are XML-escaped,
  - unsafe colors fall back to a safe default,
  - formula layers are rendered from formula metadata over the current visible viewport,
  - error bars are exported too;
- extended tests to cover extensionless import, oversized/invalid project rejection, secure project-save temp naming, SVG escaping/sanitization, plugin rediscovery, and formula-name parsing.

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
- Drag legend box: move that layer's legend
- Click title / X label / Y label: edit text inline
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

See `docs/MANAGED_INSTALL.md` for the full layout, manifest format, update workflow, and release discipline.

## Status note
The core, CLI, serializers, formula engine, importers, plugins and automated tests were rebuilt and verified in the container.
The Qt6 UI source is included and partially updated, but it was **not fully compiled in the container** because the container did not provide Qt6 development packages.
See `docs/STATUS_AND_GAPS.md`.
