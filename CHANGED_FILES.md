# CHANGED FILES (FIX BUG V8)

## Added

- `FIX_BUG_V8_REPORT.md`
- `include/plotapp/BuildInfo.h`
- `include/plotapp/ManagedInstall.h`
- `src/core/BuildInfo.cpp`
- `src/core/ManagedInstall.cpp`

## Modified

- `CMakeLists.txt`
- `VERSION.txt`
- `src/ui/SettingsDialog.h`
- `src/ui/SettingsDialog.cpp`
- `tests/tests_main.cpp`
- `README.md`
- `CHANGED_FILES.md`
- `ITERATION_REPORT.md`
- `NOT_READY.md`
- `docs/ARCHITECTURE.md`
- `docs/MANAGED_INSTALL.md`
- `docs/REQUEST_TRACEABILITY.md`
- `docs/SPECIFICATION.md`
- `docs/STATUS_AND_GAPS.md`
- `docs/TESTING.md`
- `docs/USER_GUIDE.md`

## Main theme of the revision

- add a managed-install aware GUI Updates tab in Settings;
- surface installed version / install time / installed commit in the UI;
- reuse the existing shell update engine from the GUI via `QProcess`;
- add shared version/manifest/update-status parsing utilities plus regression tests;
- synchronize the documentation with the new update workflow.
