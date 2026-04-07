# USER GUIDE

## Main workflow
1. Import data from CSV/TXT/XLSX.
2. Choose X/Y columns and, optionally, an error column.
3. Inspect the new layer in the layer tree.
4. Add a formula layer or apply a plugin to the selected source layer.
5. Drag the legend for any visible layer to a convenient place.
6. Save the project as `.plotapp` and reopen it later.

## Canvas controls
- drag inside the plot area: pan
- mouse wheel: zoom both axes
- `Shift + wheel`: X-only zoom
- `Ctrl + wheel`: Y-only zoom
- click title / X label / Y label: edit text inline
- drag legend box: move the legend for that layer

## Layer tree
- source layers appear at top level
- plugin-generated layers appear as child items below the source layer
- toggle the checkbox to hide/show the layer
- double-click a layer to open its properties dialog
- the properties dialog also contains a destructive action to delete the layer

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
- import an extra error column when loading data
- apply the `error_bars` plugin to a selected layer

Error values are stored as total error height, so an error value of `1.0` is rendered as `+0.5 / -0.5` around the point.

## Formula layers
Formula layers are stored as expressions but rendered across the *currently visible* X-range in the UI, so panning/zooming re-samples the curve for the visible plot window.

- Import and manual point entry reject non-finite numeric values such as `NaN` and `Inf`.
