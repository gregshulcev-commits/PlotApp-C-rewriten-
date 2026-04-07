# NOT READY / MANUAL VERIFICATION STILL NEEDED

The source archive is complete, but these items still need a real workstation/manual check:

1. **Full Qt6 GUI build on the target Linux desktop**
   - the container did not include Qt6 dev packages;
   - `--with-gui` correctly fails here with a clear error, but the actual GUI payload still must be built on Fedora/your target Linux host.

2. **GNOME application menu verification**
   - the desktop entry and icon generation logic are implemented;
   - in the container only CLI payloads were built, so the `.desktop` entry was intentionally skipped;
   - on the target machine verify that a GUI build produces:
     - `~/.local/share/applications/plotapp.desktop`
     - `~/.local/share/icons/hicolor/scalable/apps/plotapp.svg`
     - a visible PlotApp launcher in GNOME Applications.

3. **Real GitHub network test**
   - the update workflow was validated against a local bare git remote using the same `git ls-remote` / `git clone --depth 1 --branch ...` mechanics;
   - a real GitHub repository should still be checked on the target machine to confirm credentials/network expectations.

4. **Legacy launcher conflict handling**
   - the manager intentionally refuses to overwrite non-managed files in `~/.local/bin/plotapp*` and non-managed desktop/icon files;
   - if older manual launchers already exist on the workstation, remove them before the first managed install.

## Where to look next

- manager implementation: `tools/desktop_manager.sh`
- desktop integration template: `packaging/linux/plotapp.desktop.in`
- icon asset: `assets/icons/plotapp.svg`
- design/runtime layout notes: `docs/MANAGED_INSTALL.md`
- Fedora/manual build notes: `docs/INSTALL_FEDORA_43.md`
