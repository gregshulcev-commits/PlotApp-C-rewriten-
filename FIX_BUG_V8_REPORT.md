# FIX BUG V8 REPORT

## Goal of this pass

Add a managed-update feature to the graphical interface so the user can inspect the installed revision and trigger the existing GitHub/Git update workflow directly from **File -> Settings -> Updates**.

## What was implemented

### 1. Shared build/version information

Added:
- `include/plotapp/BuildInfo.h`
- `src/core/BuildInfo.cpp`
- `CMakeLists.txt` changes to embed the first line of `VERSION.txt` into the build.

Why:
- the UI needs to show a meaningful version even when PlotApp is launched outside a managed install;
- embedding the version at build time avoids guessing from working-directory files.

### 2. Shared managed-install metadata parsing

Added:
- `include/plotapp/ManagedInstall.h`
- `src/core/ManagedInstall.cpp`

Capabilities:
- locate likely `installation.manifest` paths;
- parse managed-install metadata such as:
  - install home,
  - system manager path,
  - installed version,
  - install time,
  - installed commit,
  - repository URL,
  - branch;
- parse the human-readable output of `desktop_manager.sh update --check-only`.

Why this design was chosen:
- it keeps the data-parsing logic testable in a headless environment;
- the GUI becomes a thin frontend over stable core helpers instead of containing ad hoc parsing code.

### 3. Settings dialog update tab

Modified:
- `src/ui/SettingsDialog.h`
- `src/ui/SettingsDialog.cpp`

New behavior:
- a dedicated **Updates** tab is now present in Settings;
- it displays:
  - build version,
  - install mode,
  - installed version,
  - install time,
  - installed commit,
  - repository,
  - branch,
  - latest remote commit,
  - current updater status;
- it contains two buttons:
  - **Check updates**
  - **Update**
- it also shows a live updater log/output pane.

### 4. GUI uses the same updater as the shell scripts

Implementation detail:
- the UI starts the stable managed-install updater with `QProcess`;
- the command path comes from the managed-install manifest (`system_manager_path`);
- the GUI runs:
  - `desktop_manager.sh update --install-home <...> --check-only`
  - `desktop_manager.sh update --install-home <...> --yes`

Why this matters:
- there is only one real update engine in the project;
- GUI and shell behavior remain aligned;
- future fixes in `desktop_manager.sh` automatically benefit both workflows.

### 5. Safety/robustness decisions in the GUI

- update commands are executed without a shell (`QProcess` program + arguments), reducing command-injection risk;
- if PlotApp is not running from a managed install, the tab stays informational and update buttons are disabled;
- the dialog blocks closing/rejecting while the updater process is running, reducing the risk of interrupting an in-flight managed reinstall;
- after a successful update, the UI explicitly instructs the user to restart PlotApp to launch the new build.

## Tests added/updated

`tests/tests_main.cpp` now also covers:
- build-version embedding from `VERSION.txt`;
- managed-install manifest parsing;
- parsing of updater status output.

## What was verified in the container

Successfully verified:
- `cmake -S . -B build -G Ninja -DPLOTAPP_BUILD_GUI=OFF`
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- AddressSanitizer + UBSan build/test run with GUI disabled

Result:
- all headless tests passed.

## What remains unverified here

Because the container does not provide Qt6 development packages or a real desktop session, these items still require a real Linux workstation:
- full Qt6 compilation of the GUI target;
- runtime verification of the **Settings -> Updates** tab;
- real GitHub-backed check/update flow from the GUI;
- restart UX after a successful GUI-triggered managed update.

## Files most relevant for the next step

- `src/ui/SettingsDialog.cpp`
- `src/ui/SettingsDialog.h`
- `src/core/ManagedInstall.cpp`
- `src/core/BuildInfo.cpp`
- `tests/tests_main.cpp`
- `docs/MANAGED_INSTALL.md`
- `NOT_READY.md`
