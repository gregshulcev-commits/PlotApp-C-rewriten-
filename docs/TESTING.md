# Testing

## Verified automatically in the current build environment

The following paths are compiled and tested:

- core project model
- CSV importer
- TXT importer
- XLSX importer
- extensionless delimited import
- malformed project rejection
- project save/load roundtrip including new layer metadata
- viewport-driven sampling for formulas and continuous derived layers
- plugin discovery, rediscovery, and execution
- derived-layer recomputation after reopen
- persistence of hidden extrema points/colors across recompute
- error-bars plugin using an imported table column
- discovery and execution of `smooth_curve` and `newton_polynomial`
- SVG rendering output, escaping, dark-theme foreground, and non-filled continuous paths
- command dispatcher basic workflow
- formula parser including scientific-notation literals

## Test command

```bash
ctest --test-dir build --output-on-failure
```

## Additional sanitizer verification used in this iteration

```bash
cmake -S . -B build-asan -G Ninja \
  -DPLOTAPP_BUILD_GUI=OFF \
  -DPLOTAPP_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' \
  -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan --output-on-failure
```

## Test data

- `examples/sample_points.csv`
- `examples/sample_noise.txt`
- `examples/sample_book.xlsx`
- temporary generated CSV fixtures inside the test build directory

## Manual desktop checks recommended on Fedora

1. Launch the GUI build with Qt6 installed.
2. Import a CSV/XLSX file and confirm the import dialog no longer asks for an error column.
3. Apply `error_bars` to an imported layer and select an imported numeric column in the plugin dialog.
4. Create a formula layer such as `sin(x)` and confirm it stays line-only while panning/zooming.
5. Apply `local_extrema` and confirm extrema are points only, with separate min/max colors.
6. Hide one extrema point in the points editor, save, reopen, and confirm the hidden point stays hidden.
7. Apply `smooth_curve` and `newton_polynomial` (change degree in the dialog).
8. Float the Layers and Command console docks, move/resize them, close them, then restore them from the toolbar/menu.
9. Export SVG and confirm the visible plot state matches the canvas.

## Why the test split looks this way

The core and plugin system are completely independent from Qt and can be verified in a headless environment.
The Qt UI requires Qt6 dev/runtime packages that are not installed in the current container, so desktop verification must be completed on Fedora.
