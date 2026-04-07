# NOT READY / MANUAL VERIFICATION STILL NEEDED

The source archive is complete and the headless core/CLI/plugin path was rebuilt and retested, but these items still need a real workstation/manual check:

1. **Full Qt6 GUI build on the target Linux desktop**
   - the container did not include Qt6 dev packages;
   - `--with-gui` correctly cannot be completed here, so the actual GUI payload still must be built on Fedora/your target Linux host.

2. **GUI import + error-bars workflow**
   - documentation was aligned with the current code: GUI import now asks only for X/Y columns;
   - confirm on a real Qt6 desktop that `error_bars` can be applied after import and the plugin dialog exposes imported numeric columns correctly.

3. **GNOME application menu verification**
   - the desktop entry/icon generation logic was hardened;
   - in the container only CLI payloads were built, so the `.desktop` entry was intentionally skipped;
   - on the target machine verify that a GUI build produces:
     - `~/.local/share/applications/plotapp.desktop`
     - `~/.local/share/icons/hicolor/scalable/apps/plotapp.svg`
     - a visible PlotApp launcher in GNOME Applications;
   - additionally confirm that the launcher still works when the user/home path contains spaces or characters such as `&`.

4. **Real GitHub network test**
   - the update workflow was validated against a local bare git remote using the same `git ls-remote` / `git clone --depth 1 --branch ...` mechanics;
   - a real GitHub repository should still be checked on the target machine to confirm credentials/network expectations.

5. **Plugin trust boundary**
   - plugin result validation was hardened earlier, but plugins are still trusted native code loaded into the process;
   - isolating plugins into a separate process/sandbox was not implemented in this pass.

## Where to continue from here

- project serializer hardening: `src/serialization/ProjectSerializer.cpp`
- save/load regression tests: `tests/tests_main.cpp`
- managed install desktop generation: `tools/desktop_manager.sh`
- user-facing docs synchronized with current behavior: `docs/USER_GUIDE.md`, `docs/TESTING.md`, `docs/MANAGED_INSTALL.md`
- audit summary: `FIX_BUG_V7_REPORT.md`
