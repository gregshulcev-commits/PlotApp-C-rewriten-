# USER GUIDE

## Main workflow
1. Import data from CSV/TXT/XLSX or create a manual/formula layer.
2. In the layer tree, select **exactly one** source layer.
3. By default, selecting a layer selects the whole layer as the plugin input.
4. Optionally restrict the input with `Shift + left drag` on the plot to select only the points that fall inside the rectangle for the currently selected layer.
5. Apply a plugin to the selected layer. The plugin receives only that layer and only the currently selected subset of its points.
6. Save the project as `.plotapp` and reopen it later. The selected source subset used by a derived layer is persisted and reused on recompute.

## Canvas controls
- drag inside the plot area: pan
- mouse wheel: zoom both axes
- `Shift + wheel`: X-only zoom
- `Ctrl + wheel`: Y-only zoom
- `Shift + left drag`: rectangular point selection for the currently selected layer
- `Esc`: clear the current layer selection and point selection
- click title / X label / Y label: edit text inline
- drag legend box: move the legend for that layer

## Layer tree
- source layers appear at top level
- plugin-generated layers appear as child items below the source layer
- the tree uses **single selection** for plugin targeting
- clicking a layer selects that one layer and resets the point selection to the full layer
- pressing `Esc` clears the current tree/canvas selection and removes the bold selected-layer highlight from the plot
- toggle the checkbox to hide/show the layer
- double-click a layer to open its properties dialog
- the properties dialog also contains a destructive action to delete the layer

## Default names and legend text
- imported layers default to `Y vs X` using the selected column headers
- formula layers default to the formula expression itself
- the default legend text follows the same rule as the default layer name
- you can still override the layer name and legend text manually in the properties dialog

## Point selection and plugins
- plugins are applied to **one selected layer only**
- a rectangle selection affects only the layer currently selected in the tree
- if no rectangle is drawn, the full layer remains selected
- formula layers are treated as whole-layer selections when chosen in the tree; their stored sample points can also be restricted with rectangular selection if needed
- derived layers remember which source points were used, so reopening the project and recomputing keeps the same plugin input subset

## Exporting images
Use **File -> Export image...**.

The export dialog combines both raster and vector export into one place:
- choose **PNG** or **SVG**
- choose one of the size presets:
  - **Current canvas size**
  - **A4 portrait**
  - **A4 landscape**
  - **Custom**
- adjust **DPI** for A4/print-oriented exports
- inspect the preview before writing the file

Notes:
- PNG uses the selected pixel size directly;
- SVG remains vector output, but the preview shows the same aspect ratio and composition;
- A4 presets are useful when you want an export that already matches a printable page size.

## Role / min-max layers
`Role` is no longer treated as a global point attribute.

- the point editor exposes the **Role** column only for the `local_extrema` (min/max) plugin layer
- legends only use the secondary role color for the extrema layer
- raw layers and unrelated derived layers no longer show or interpret `Role`

## Console
The built-in command console is a UI wrapper around the same command dispatcher used by the CLI.

Useful commands:
- `help`
- `pwd`
- `cd <dir>`
- `ls`
- `plugins`
- `import <path> <x> <y> [name] [error_column]`
- `formula "sin(x)" -10 10 600 wave`
- `apply-plugin local_extrema <layer_id> mode=both;window=3;tolerance=0.0`
- `!ls -la` for optional bash commands after enabling `PLOTAPP_ENABLE_SHELL=1`

Press `Tab` in the command line to complete built-in PlotApp commands.

## Error bars
There are two ways to work with error bars:
- in the CLI, `import <path> <x> <y> [name] [error_column]` still accepts an optional error column for backward compatibility;
- in the GUI, import the table normally and then apply the `error_bars` plugin to the selected layer, choosing a numeric imported column in the plugin dialog.

Error values are stored as total error height, so an error value of `1.0` is rendered as `+0.5 / -0.5` around the point.
Bounds and SVG export now account for the full error-bar height.

## Linear approximation plugin
The `linear_fit` plugin now has an extra desktop option:
- **Show intersections with the X and Y axes**

When enabled, the fitted line is extended so that the exported/visible line can include the intercepts with the axes.

## Formula layers
Formula layers are stored as expressions but rendered across the *currently visible* X-range in the UI, so panning/zooming re-samples the curve for the visible plot window.

Important behavior:
- the formula dialog now starts from the **current visible X-range** when the project already contains data;
- the formula-layer name defaults to the formula expression, and the legend text does the same;
- adding a formula layer to an existing project no longer forcibly resets the viewport, which avoids the common `exp(x)` “huge Y scale collapses everything” problem;
- exponentiation binds tighter than unary minus, so `-x^2` is parsed as `-(x^2)`.

- Import and manual point entry reject non-finite numeric values such as `NaN` and `Inf`.
- `.plotapp` loading rejects oversized/non-regular files and caps per-layer point/table payloads to reduce memory-exhaustion risk.

## Settings -> Updates
- open **File -> Settings -> Updates** when PlotApp is running from a managed installation;
- **Build version** comes from the version embedded at build time;
- **Installed version**, **Installed at**, and **Installed commit** come from `~/.local/share/plotapp-install/metadata/installation.manifest`;
- **Check updates** runs the same managed-update check as `update_app.sh --check-only`;
- **Update** runs the same managed reinstall as `update_app.sh --yes`;
- after a successful update, restart PlotApp to launch the new build;
- if PlotApp was started from an unmanaged/development run, the tab stays informational and the update buttons remain disabled.
