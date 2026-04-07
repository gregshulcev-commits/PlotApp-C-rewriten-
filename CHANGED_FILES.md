# CHANGED FILES (FIX BUG V6)

## Added

- `install_app.sh`
- `update_app.sh`
- `uninstall_app.sh`
- `tools/desktop_manager.sh`
- `docs/MANAGED_INSTALL.md`
- `assets/icons/plotapp.svg`
- `packaging/linux/plotapp.desktop.in`
- `FIX_BUG_V6_REPORT.md`

## Modified

- `CMakeLists.txt`
- `README.md`
- `src/app/main.cpp`
- `VERSION.txt`
- `NOT_READY.md`

## Main theme of the revision

- managed installation layout with `current`/`previous` payloads;
- shell-first manifest + JSON mirror;
- GitHub-style update via fresh clone instead of `git pull` in the installed copy;
- user launchers, update/uninstall wrappers, desktop entry, and icon generation;
- rollback-oriented publish flow.
