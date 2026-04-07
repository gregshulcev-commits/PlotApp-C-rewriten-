# CLI COMMANDS

## Built-in commands

- `help` — show command help
- `pwd` — show current working directory
- `cd <dir>` — change command-console working directory
- `ls [dir]` — list files/directories
- `new-project` — reset current project
- `title <text>` — set project title
- `labels <x_label> <y_label>` — set axis labels
- `import <path> <x> <y> [name] [error_column]` — import tabular data
- `formula <expr> <xmin> <xmax> [samples] [name]` — create a formula layer
- `list-layers` — list current layers
- `add-point <layer_id> <x> <y>` — append one finite point (`NaN`/`Inf` are rejected)
- `apply-plugin <plugin_id> <layer_id> [params]` — build a derived layer
- `toggle-layer <layer_id>` — show/hide a layer
- `save <path>` — save project (adds `.plotapp` if needed)
- `open <path>` — open project and recompute derived layers when possible
- `export-svg <path>` — export visible state to SVG
- `plugins` — list discovered plugins
- `!<bash command>` — explicit shell passthrough in the current console directory (disabled by default; enable with `PLOTAPP_ENABLE_SHELL=1`)

## Examples

```bash
./build/plotapp-cli help
./build/plotapp-cli "import ./examples/sample_points.csv 0 1 raw"
./build/plotapp-cli "import ./measurements 0 1 raw"
./build/plotapp-cli "formula x^2 -2 2 parabola"
./build/plotapp-cli "formula sin(x) -10 10 1024 wave"
./build/plotapp-cli "apply-plugin smooth_curve <layer_id> samples=512"
./build/plotapp-cli "apply-plugin newton_polynomial <layer_id> degree=3;samples=512"
./build/plotapp-cli "apply-plugin error_bars <layer_id> column_index=2"
./build/plotapp-cli "plugins"
```

## Notes

- `formula x^2 -2 2 parabola` is valid: the sample count is optional.
- files without extension can be imported when they look like delimited text data.
- non-finite numeric values such as `NaN` and `Inf` are rejected instead of being imported into the project.
- GUI import no longer asks for an error column; the CLI keeps `[error_column]` only for backward compatibility.
- the shell passthrough is intentionally explicit: PlotApp commands keep priority, shell commands require the `!` prefix, and shell execution is disabled until `PLOTAPP_ENABLE_SHELL=1` is set.
