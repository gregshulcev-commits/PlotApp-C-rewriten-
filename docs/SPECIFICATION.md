# Technical specification

## Goal

Create an extendable desktop application for plotting and analyzing graphs from experimental data.
The primary target platform is Fedora Workstation 43 on a ThinkPad T14 Gen 2 AMD class machine.

## Functional requirements

### Data input

1. The application shall import data from CSV files.
2. The application shall import data from TXT files with whitespace, comma, semicolon, or tab separators.
3. The application shall import data from XLSX workbooks.
4. The import workflow shall preview file columns and allow the user to pick X and Y columns.
5. The application shall allow manual point editing for an existing layer.

### Layer model

1. Every plotted series shall be represented as a layer.
2. A layer shall have at least: id, name, visibility flag, type, style, points, provenance metadata.
3. Layers shall be independently visible or hidden.
4. Derived layers shall keep a reference to their source layer and generator plugin.
5. Derived layers shall still preserve generated points inside the project file so a project remains reopenable even when a plugin is unavailable.

### Plugins and approximations

1. Approximations shall be implemented as separately compiled shared libraries.
2. The core application shall discover plugins at runtime.
3. Plugins shall appear in the UI without modifying the core application.
4. Plugins shall receive a source layer and parameter string.
5. Plugins shall return a new derived layer dataset.

### Project persistence

1. The application shall save the whole project into a single file.
2. The project file shall persist project settings, layers, points, visibility, style, and derivation metadata.
3. The application shall reopen a saved project and restore the working state.
4. The application shall attempt to recompute derived layers when the required plugin is available.
5. If a plugin is not available, the application shall keep the stored generated points and surface a warning.

### Rendering and export

1. The desktop application shall show the graph inside the main application window.
2. The application shall support visible-layer export to image format.
3. The core shall support headless SVG export for testing and automation.
4. The Qt UI shall support PNG export from the current canvas.

### UI and command wrapper

1. The desktop application shall provide a graphical interface.
2. The UI shall expose a layer list with visibility toggles.
3. The UI shall expose import, open, save, plugin, and export actions.
4. The UI shall include a command console acting as a simplified wrapper around CLI commands.
5. When running from a managed installation, the settings dialog shall expose an Updates tab with installed version, install time, installed commit, and buttons to check/install updates using the managed-update workflow.

## Non-functional requirements

1. The core logic shall remain modular and reusable without GUI dependencies.
2. The plugin ABI shall stay stable and simple.
3. The source base shall compile as C++17.
4. The code shall be structured so new importers, renderers, or plugins can be added independently.
5. The project shall favor explicit documentation and inspectable text formats for the first MVP.

## MVP scope

Included:
- CSV/TXT/XLSX import
- raw and derived layers
- two example plugins: linear fit and moving average
- save/open project
- visible layer export
- Qt6 desktop shell and command console source

Deferred beyond current MVP:
- logarithmic axes
- undo/redo stack
- advanced table editor with validation hints
- multi-sheet XLSX chooser
- error bars as a first-class plugin
- 3D plots
