# Iteration Report — fix bug v5

This continuation focused on bug fixing, security hardening, and reproducible verification of the core/CLI path.

## Completed in this pass
- rejected formula layers that produce fewer than two finite points;
- disabled shell passthrough by default and required explicit opt-in via `PLOTAPP_ENABLE_SHELL=1`;
- rejected non-finite numeric values (`NaN`/`Inf`) during project load;
- normalized malformed custom viewport values loaded from project files;
- rejected non-finite numeric input in generic numeric parsing, importer-driven extraction, plugin numeric params, and manual point insertion;
- hardened SVG/canvas bounds handling against hidden and non-finite points;
- added/updated regression tests for all of the above.

## Verified here
- headless build of core/CLI/plugins;
- `ctest` passed;
- AddressSanitizer + UBSan build/test passed;
- manual reproduction of the previously broken formula/project/shell scenarios.

## Not fully verified here
- full Qt6 desktop build and runtime behavior.

## Reason
- the container used for this work does not provide Qt6 development packages.

## See also
- `FIX_BUG_V5_REPORT.md`
- `NOT_READY.md`
