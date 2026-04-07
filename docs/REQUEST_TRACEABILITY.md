# REQUEST TRACEABILITY

This file maps the latest requested changes to the current source update.

1. Formula input like `2sin(x)` must not hang/crash the program.
   - implemented in `src/core/FormulaEvaluator.cpp` by adding implicit multiplication handling for common cases.

2. Invalid formula input should show a warning and let the user correct it.
   - implemented in `include/plotapp/FormulaEvaluator.h`, `src/core/FormulaEvaluator.cpp`, `src/core/ProjectController.cpp`, `src/ui/FormulaLayerDialog.*`, `src/ui/LayerPropertiesDialog.*`, and `src/ui/MainWindow.cpp`.
   - creation/editing now validate first and only commit changes after successful regeneration.

3. A failed formula creation/edit must not leave a broken layer behind that later crashes rendering.
   - implemented in `src/core/ProjectController.cpp` and protected additionally in `src/core/LayerSampler.cpp`.

4. Parent layers should be hideable without forcing child layers to become hidden too.
   - implemented in `src/ui/MainWindow.cpp` by removing recursive visibility propagation in the layer tree.
   - the project-level independent visibility behavior is also covered by tests in `tests/tests_main.cpp`.
