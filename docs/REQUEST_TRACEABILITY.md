# REQUEST TRACEABILITY

This file maps the latest requested changes to the current source update.

1. Add update controls to the graphical interface.
   - implemented in `src/ui/SettingsDialog.*` by adding a dedicated **Updates** tab.
   - the tab exposes **Check updates** and **Update** buttons and a live updater log.

2. Show the currently installed version, install time, and GitHub commit in the settings UI.
   - implemented through shared managed-install metadata parsing in `include/plotapp/ManagedInstall.h`, `src/core/ManagedInstall.cpp`, and the new UI bindings in `src/ui/SettingsDialog.cpp`.
   - build-time version embedding was added via `include/plotapp/BuildInfo.h`, `src/core/BuildInfo.cpp`, and `CMakeLists.txt`.

3. Keep GUI updates aligned with the existing shell/managed-install workflow.
   - implemented in `src/ui/SettingsDialog.cpp` by invoking the stable `desktop_manager.sh update` flow with `QProcess` instead of duplicating update logic in C++.
   - documented in `docs/MANAGED_INSTALL.md` and `docs/ARCHITECTURE.md`.

4. Keep the change testable even without Qt in the container.
   - implemented by moving version/manifest/update-status parsing into the shared core layer and covering it in `tests/tests_main.cpp`.
