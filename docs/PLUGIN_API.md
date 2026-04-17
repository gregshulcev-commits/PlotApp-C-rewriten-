# Plugin API

For the full step-by-step authoring guide in Russian, see **`docs/PLUGIN_AUTHORING_RU.md`**.

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

## Practical authoring checklist

When you create a plugin, follow this order:

1. Pick a **stable plugin id**. It is stored in project files, so changing it later breaks recompute compatibility.
2. Decide what the plugin consumes:
   - all source points,
   - or the subset already filtered by the desktop selection.
3. Decide what parameters you want in the `key=value;key=value` string.
4. Return a suggested layer name that helps the user understand the result.
5. Make sure `plotapp_free_result` frees **everything** allocated in `plotapp_run`.
6. Drop the resulting `.so` into the plugin search directory and reload plugins.

## Parameter-string convention

The core treats `params` as an opaque string. A plugin can define its own convention, but the repository uses this style consistently:

- `key=value`
- pairs separated by `;`
- examples:
  - `samples=128`
  - `degree=3;samples=200`
  - `window=5;tolerance=0.1`
  - `show_axis_intersections=1`

Recommendations:
- keep names lowercase with underscores;
- choose safe defaults when a key is missing;
- ignore unknown keys instead of failing unless the plugin really cannot proceed.

## Result-shape recommendations

The ABI only returns points, a suggested layer name, and an optional warning string. The core then decides how to style the derived layer.

That means:
- if your plugin returns a continuous curve, emit points ordered by `x` and let the core connect them;
- if your plugin returns marker-like data, emit only the important points;
- keep the number of generated points reasonable for interactivity and project-file size.

## Memory-management rules

Inside `plotapp_run`:
- allocate returned arrays/strings with `malloc` / `std::malloc`;
- never return pointers to local variables or `std::string::c_str()` storage;
- initialize unused result fields to `nullptr` / `0`.

Inside `plotapp_free_result`:
- `free(result->points)`
- `free(result->suggested_layer_name)`
- `free(result->warning_message)`
- reset the pointers to `nullptr`
- reset `point_count` to `0`

## UI integration note

The current desktop UI can add plugin-specific controls without changing the ABI.
Example: `linear_fit` now exposes a desktop checkbox that writes `show_axis_intersections=1` into the existing parameter string.

This means you can often extend a plugin by:
1. adding a new optional parameter in the plugin itself;
2. teaching the plugin dialog to write that parameter;
3. leaving the ABI unchanged.

## Compatibility note

Because selection support is implemented in the core by filtering the source-layer view, existing plugins remain ABI-compatible. They automatically work with full-layer or subset-based execution without recompiling for a new API version.
