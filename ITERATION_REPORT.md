# Iteration Report — fix bug v7

This continuation focused on a deeper source audit of project-file save/load behavior, managed-install desktop generation, documentation drift, and reproducible verification of the core/CLI path.

## Completed in this pass
- replaced predictable `<project>.tmp` save behavior with unique adjacent temp-file creation plus atomic rename;
- rejected oversized and non-regular `.plotapp` inputs before parsing;
- capped project layers, points, imported rows/columns, and line length during load;
- added save-side validation so unsupported oversized projects are rejected before serialization;
- hardened managed desktop entry generation so launcher paths with spaces or `&` remain valid;
- added cleanup state for temporary managed-install artifacts on early failure;
- synchronized user/docs with the current GUI error-bar workflow and current plugin set;
- added/updated regression tests for secure save temp naming and project resource-limit rejection.

## Verified here
- headless build of core/CLI/plugins;
- `ctest` passed;
- AddressSanitizer + UBSan build/test passed;
- shell syntax validation via `bash -n`;
- targeted desktop-entry generation check with a launcher path containing spaces and `&`.

## Not fully verified here
- full Qt6 desktop build and runtime behavior;
- real GNOME launcher behavior on a workstation;
- real GitHub network update flow.

## Reason
- the container used for this work does not provide Qt6 development packages or a real desktop session.

## See also
- `FIX_BUG_V7_REPORT.md`
- `NOT_READY.md`
