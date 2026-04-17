# Plugin API

The stable plugin boundary is defined in `include/plotapp/PluginApi.h`.

## Contract

A plugin must export three C symbols:
- `plotapp_get_metadata`
- `plotapp_run`
- `plotapp_free_result`

## Metadata

A plugin describes itself with:
- API version
- plugin id
- display name
- description
- default params string

## Execution model

Input:
- exactly one source-layer view
- parameter string

Output:
- generated points
- optional suggested layer name
- optional warning text

## Selection semantics

The ABI did **not** need to change to support point selection.

The core implements selection-aware plugins by preparing the source-layer view before calling the plugin:
- the UI selects one layer only;
- an optional rectangular point selection can restrict the source to a subset of that layer;
- `ProjectController` builds a temporary source-layer view that contains only the selected points;
- the plugin receives that filtered view as its normal input.

From a plugin's perspective, it simply operates on `request->source_layer.points[0..point_count)`.
It does not need to know whether the user selected the full layer or only part of it.

## Derived-layer provenance

After plugin execution, the core stores derived-layer provenance separately from the ABI payload:
- source layer id
- plugin id
- plugin params
- selected source-point indices (empty means “the whole source layer”)

This allows saving/reopening/recomputing a project without changing the plugin ABI.

## ABI rules

- use `extern "C"`
- allocate result buffers with `malloc`
- release result buffers in `plotapp_free_result`
- return non-zero on failure
- do not retain pointers passed in the request after `plotapp_run` returns

## Point roles

`pointRoles` are not a general-purpose plugin feature in the UI.
At the moment they are interpreted only for the `local_extrema` / min-max workflow.
Plugins that do not implement extrema-style semantics should not expect role-aware rendering or role editing in the desktop UI.

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

## Compatibility note

Because selection support is implemented in the core by filtering the source-layer view, existing plugins remain ABI-compatible. They automatically work with full-layer or subset-based execution without recompiling for a new API version.
