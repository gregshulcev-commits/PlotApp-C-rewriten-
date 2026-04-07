# Iteration Report — fix bug v8

This continuation focused on adding a managed-update feature to the Qt settings UI without splitting the update logic away from the existing shell workflow.

## Completed in this pass
- embedded `VERSION.txt` into the build as shared runtime build info;
- added shared managed-install manifest parsing and update-status parsing utilities in the core layer;
- extended the Qt settings dialog with a dedicated **Updates** tab;
- surfaced build version, installed version, install time, installed commit, repository, branch, and remote commit/status in the dialog;
- connected **Check updates** and **Update** to the existing `desktop_manager.sh update` flow through `QProcess`;
- blocked dialog closing while a managed update process is running, reducing the risk of interrupting an in-flight reinstall;
- updated documentation so the GUI update workflow matches the managed install architecture;
- added regression tests for build-version embedding, manifest parsing, and updater status parsing.

## Verified here
- headless build of core/CLI/plugins;
- `ctest` passed;
- AddressSanitizer + UBSan build/test passed.

## Not fully verified here
- full Qt6 desktop build and runtime behavior;
- real GitHub network update flow via the GUI buttons;
- final restart UX on a real workstation after a successful GUI-triggered managed update.

## Reason
- the container used for this work does not provide Qt6 development packages, a desktop session, or a real GitHub-connected managed installation.

## See also
- `FIX_BUG_V8_REPORT.md`
- `NOT_READY.md`
