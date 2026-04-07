# Development process log

## Inputs reviewed

1. The original C++ archive mostly contained an architectural sketch and a minimal Qt entry point.
2. The reference archive showed the desired import workflow for CSV/TXT/XLSX and column selection behavior.

## Design decisions

### Decision 1: split the project into a reusable core plus a Qt shell

Reason:
- the user explicitly asked for modularity
- the user also wanted a UI that can act as a wrapper over a simpler terminal flow

### Decision 2: model everything as layers

Reason:
- raw datasets and approximations must coexist
- visibility and style must be controlled independently
- the project file must persist all visible and hidden work

### Decision 3: make approximations runtime plugins

Reason:
- the user explicitly wanted new approximation modules to appear without changing main application code

### Decision 4: store both derivation recipe and generated points

Reason:
- reopening projects must work even without a plugin
- reproducibility is better when both source metadata and generated output are retained

### Decision 5: keep a headless renderer

Reason:
- it enables automated testing and CI-friendly exports
- it reduces coupling between rendering logic and Qt UI availability

## Implementation steps

1. Define the core data model and persistence format.
2. Implement importers for CSV/TXT/XLSX.
3. Implement runtime plugin loader and two example plugins.
4. Implement project controller and command dispatcher.
5. Implement headless SVG renderer.
6. Implement Qt UI source as a thin desktop shell over the controller.
7. Add automated tests for the verifiable non-Qt path.
8. Write documentation and status report.
