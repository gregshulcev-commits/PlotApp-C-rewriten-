# FIX BUG V6 REPORT

## Summary

This revision adds a shell-first managed installation/update/removal layer for PlotApp and adapts the GitHub update principle to the existing C++/CMake/Qt architecture.

Main result:
- the installed application no longer needs to run from the unpacked source directory;
- updates are performed through `git ls-remote` + fresh clone + rebuild + managed publish;
- the manager keeps `current` and `previous` payload directories for safer rollback;
- user runtime data and cache are kept separate from the payload;
- GNOME desktop integration (desktop entry + icon) is generated for GUI builds.

## Added files

- `install_app.sh`
- `update_app.sh`
- `uninstall_app.sh`
- `tools/desktop_manager.sh`
- `docs/MANAGED_INSTALL.md`
- `assets/icons/plotapp.svg`
- `packaging/linux/plotapp.desktop.in`

## Updated files

- `CMakeLists.txt`
- `README.md`
- `src/app/main.cpp`
- `VERSION.txt`
- `CHANGED_FILES.md`
- `NOT_READY.md`

## Key implementation notes

### 1. Managed install home

Default managed layout:
- `~/.local/share/plotapp-install/app/current`
- `~/.local/share/plotapp-install/app/previous`
- `~/.local/share/plotapp-install/metadata/installation.manifest`
- `~/.local/share/plotapp-install/metadata/installation.json`
- `~/.local/share/plotapp-install/system/desktop_manager.sh`

### 2. Two manifest files

- `installation.manifest` is the canonical shell-readable manifest.
- `installation.json` is a JSON mirror for diagnostics/future integrations.

### 3. Update workflow

`update_app.sh` now:
1. loads the manifest;
2. checks remote commit via `git ls-remote`;
3. clones the tracked branch into a temporary directory;
4. rebuilds and installs into a staging payload;
5. promotes staging to `current`;
6. moves the old `current` payload to `previous`.

### 4. Safety measures

- staging payload before publish;
- manifest written via temp file + replace;
- rollback to the old payload on publish errors;
- non-managed wrappers/desktop files are not overwritten;
- explicit `--yes` support for non-interactive updates/uninstall.

### 5. GNOME integration

If the GUI binary `plotapp` exists in the managed payload, the manager installs:
- `~/.local/share/applications/plotapp.desktop`
- `~/.local/share/icons/hicolor/scalable/apps/plotapp.svg`

If only CLI is built, the installer keeps the CLI wrappers and warns that no GNOME menu entry was created.

## Validation performed

### Build/tests

Validated in container:
- `cmake -S . -B build -G Ninja -DPLOTAPP_BUILD_GUI=OFF -DPLOTAPP_BUILD_TESTS=ON`
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`

### Managed install/update/uninstall scenario

Validated with a local git remote (GitHub-like flow without network access):
- install from a git working tree;
- `--check-only` when already up to date;
- commit/push a newer revision to the remote;
- update from a fresh clone;
- verify `current` + `previous` payloads;
- uninstall with `--purge-data`.

### Archive install + bind update source

Validated separately:
- install from a source directory without `.git`;
- bind remote with `update_app.sh --set-repo <repo> --branch main`;
- verify manifest update;
- verify `--check-only` output when installed commit is unknown.

## Remaining manual checks

See `NOT_READY.md`.
