# NOT READY / MANUAL VERIFICATION STILL NEEDED

The source archive is complete and the headless core/CLI/plugin path was rebuilt and retested, but these items still need a real workstation/manual check:

1. **Full Qt6 GUI build on the target Linux desktop**
   - the container did not include Qt6 dev packages;
   - the new `SettingsDialog` source was added, but the full GUI target still must be built on Fedora/your target Linux host.

2. **Settings -> Updates tab runtime verification**
   - confirm that the tab opens correctly, that labels wrap/select text properly, and that the dialog remains responsive while `QProcess` runs;
   - verify that the dialog cannot be closed during a running update/check in ways that would terminate the updater unexpectedly.

3. **Managed-install metadata display**
   - verify on the target machine that the tab shows:
     - build version,
     - installed version,
     - install time,
     - installed commit,
     - repository and branch,
     - latest remote commit after a check;
   - verify fallback behavior when PlotApp is started from an unmanaged/dev build.

4. **Real GitHub network update test from the GUI**
   - click **Check updates** against the real repository and confirm the status matches `update_app.sh --check-only`;
   - click **Update** against a testable newer revision and confirm the managed payload is replaced successfully;
   - after success, confirm that restarting PlotApp launches the new build and the updated manifest values are shown.

5. **GNOME application menu verification**
   - confirm that a GUI build still produces:
     - `~/.local/share/applications/plotapp.desktop`
     - `~/.local/share/icons/hicolor/scalable/apps/plotapp.svg`
     - a visible PlotApp launcher in GNOME Applications;
   - confirm that launcher/update integration still works when the user/home path contains spaces or characters such as `&`.

6. **Plugin trust boundary**
   - plugins are still trusted native code loaded into the process;
   - isolating plugins into a separate process/sandbox was not implemented in this pass.

## Where to continue from here

- GUI update integration: `src/ui/SettingsDialog.cpp`, `src/ui/SettingsDialog.h`
- shared manifest/build-info parsing: `src/core/ManagedInstall.cpp`, `src/core/BuildInfo.cpp`
- regression tests: `tests/tests_main.cpp`
- managed install/update documentation: `docs/MANAGED_INSTALL.md`, `docs/USER_GUIDE.md`, `docs/TESTING.md`
- audit summary: `FIX_BUG_V8_REPORT.md`
