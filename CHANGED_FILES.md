# CHANGED FILES (FIX BUG V7)

## Added

- `FIX_BUG_V7_REPORT.md`

## Modified

- `src/serialization/ProjectSerializer.cpp`
- `tests/tests_main.cpp`
- `tools/desktop_manager.sh`
- `README.md`
- `VERSION.txt`
- `CHANGED_FILES.md`
- `NOT_READY.md`
- `ITERATION_REPORT.md`
- `docs/USER_GUIDE.md`
- `docs/STATUS_AND_GAPS.md`
- `docs/TESTING.md`
- `docs/MANAGED_INSTALL.md`
- `docs/PLUGIN_API.md`

## Main theme of the revision

- deep source audit for project-file handling, managed install generation, and documentation consistency;
- secure `.plotapp` save/load hardening with predictable-temp removal and resource caps;
- safer GNOME desktop entry rendering for managed installs;
- documentation sync with the current GUI/CLI error-bar workflow and current plugin set.
