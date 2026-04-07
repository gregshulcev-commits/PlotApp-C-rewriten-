# Plugin API

The stable plugin boundary is defined in `include/plotapp/PluginApi.h`.

## Contract

A plugin must export three C symbols:

- `plotapp_get_metadata`
- `plotapp_run`
- `plotapp_free_result`

## Metadata

A plugin describes itself with:
- api version
- plugin id
- display name
- description
- default params string

## Execution model

Input:
- one source layer view
- parameter string

Output:
- generated points
- optional suggested layer name
- optional warning text

## ABI rules

- use `extern "C"`
- allocate result buffers with `malloc`
- release result buffers in `plotapp_free_result`
- return non-zero on failure

## Example plugins in this repository

Current in-tree examples include:
- `linear_fit` — least-squares straight-line approximation
- `moving_average` — simple smoothing for noisy data
- `local_extrema` — min/max point extraction with per-point roles
- `smooth_curve` — densified smooth curve generation
- `error_bars` — attach per-point errors from uniform input or imported columns
- `newton_deg2`, `newton_deg4`, `newton_deg5` — fixed-degree Newton interpolation examples
- `newton_polynomial` — configurable Newton interpolation

## Adding a new plugin

1. Create a new shared-library target under `plugins/<name>/`.
2. Include `plotapp/PluginApi.h`.
3. Export the required symbols.
4. Build the shared library into the `plugins/` output directory.
5. Restart the app or reload plugins from the UI.
