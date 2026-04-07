# FIX BUG V7 REPORT

## Summary

This revision performs a deeper audit of PlotApp with emphasis on `.plotapp` project-file handling, managed-install desktop generation, automated regression coverage, and documentation consistency.

Main result:
- project save/load is hardened against predictable-temp overwrite patterns and oversized/hostile `.plotapp` inputs;
- managed GNOME desktop generation no longer performs unescaped placeholder substitution for launcher paths/text fields;
- documentation now matches the current GUI error-bar workflow and the current in-tree plugin set;
- the updated source tree includes regression tests for the newly fixed save/load cases.

## High-confidence findings from this audit

### 1. Predictable project temp path during save

Previous behavior:
- `src/serialization/ProjectSerializer.cpp` always wrote to `<project>.tmp` before rename.

Risk:
- predictable sidecar names are easier to race, pre-create, or abuse in untrusted writable directories;
- they also collide with any unrelated file already using the same `.tmp` suffix.

Fix:
- save now creates a unique adjacent temporary file with exclusive creation semantics, flushes it, and renames it into place.

### 2. Unbounded `.plotapp` load path

Previous behavior:
- `.plotapp` loading had no file-size cap, no layer cap, no point cap, and no imported-table row/column limits;
- non-regular file paths were not rejected early.

Risk:
- a crafted project file could trigger excessive memory use or hangs by feeding huge point/table payloads or special files.

Fix:
- reject non-regular project paths before parsing;
- reject oversized `.plotapp` files before parsing;
- cap layers, points, imported rows, imported columns, and line length during load;
- reject out-of-range imported column indices;
- apply matching save-side validation so the application does not write projects that violate the supported safety envelope.

### 3. Managed desktop entry placeholder substitution

Previous behavior:
- `tools/desktop_manager.sh` used raw `sed` substitution to inject launcher paths and text fields into the `.desktop` template.

Risk:
- paths containing characters such as `&`, backslashes, or spaces could corrupt the generated desktop entry or produce a launcher that does not start.

Fix:
- render the desktop template via escaped placeholder substitution so special characters stay literal;
- sanitize text fields and quote the `Exec=` launcher token.

### 4. Installer temporary-artifact cleanup on early failure

Previous behavior:
- explicit cleanup existed on several rollback branches, but early exits before the normal success path could leave temporary build/staging/backup artifacts behind.

Fix:
- managed install now registers cleanup state so temporary build/staging/backup artifacts are removed automatically on early install failure.

### 5. Documentation drift

Findings:
- `docs/USER_GUIDE.md` still claimed the GUI import dialog optionally accepted an error column, which no longer matched the current UI;
- `docs/STATUS_AND_GAPS.md` still referenced `fix bug v5`;
- `docs/PLUGIN_API.md` listed only an outdated subset of the plugins currently shipped in-tree.

Fix:
- synchronized user guide, status notes, testing notes, managed-install notes, plugin API examples, and top-level README with the current implementation.

## Files changed in this pass

- `src/serialization/ProjectSerializer.cpp`
- `tests/tests_main.cpp`
- `tools/desktop_manager.sh`
- `README.md`
- `VERSION.txt`
- `CHANGED_FILES.md`
- `NOT_READY.md`
- `docs/USER_GUIDE.md`
- `docs/STATUS_AND_GAPS.md`
- `docs/TESTING.md`
- `docs/MANAGED_INSTALL.md`
- `docs/PLUGIN_API.md`

## Validation performed in container

### Build and tests

Validated successfully:
- `cmake -S . -B build -G Ninja -DPLOTAPP_BUILD_GUI=OFF -DPLOTAPP_BUILD_TESTS=ON`
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`

### Sanitizers

Validated successfully:
- `cmake -S . -B build_asan -G Ninja -DPLOTAPP_BUILD_GUI=OFF -DPLOTAPP_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address,undefined'`
- `cmake --build build_asan`
- `ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build_asan --output-on-failure`

### Additional checks

- shell syntax validation for install/update/uninstall scripts via `bash -n`;
- source/documentation diff review for GUI import, CLI commands, plugin list, and managed-install notes.

## Remaining gaps

See `NOT_READY.md` for the workstation-side checks that still require a real Qt6 desktop and a real GitHub network path.
