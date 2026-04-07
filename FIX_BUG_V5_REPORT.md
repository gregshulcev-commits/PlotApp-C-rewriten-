# Fix Bug v5 Report

## Что я проверил

Проверка и воспроизведение:
- headless-сборка core/CLI/plugins;
- автотесты через `ctest`;
- дополнительная проверка через AddressSanitizer + UBSan;
- ручное воспроизведение критичных сценариев:
  - `formula 1/0 -1 1 bad`;
  - открытие `.plotapp` с `POINT=nan,1`;
  - запуск `!echo test` из CLI/консоли.

Команды, которые были успешно прогнаны:
```bash
cmake -S . -B build -G Ninja -DPLOTAPP_BUILD_GUI=OFF
cmake --build build
ctest --test-dir build --output-on-failure

cmake -S . -B build-asan -G Ninja -DPLOTAPP_BUILD_GUI=OFF \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' \
  -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

## Найденные и исправленные критичные/важные баги

### 1) Формула могла создать «успешный» слой без валидных точек
Симптом: формула вида `1/0` проходила часть обработки и создавала пустой/битый слой вместо ошибки.

Исправлено:
- теперь формульный слой создаётся только если в диапазоне получено минимум две конечные точки;
- при невалидной формуле или невалидном результате слой в проект не попадает.

Файлы:
- `src/core/FormulaEvaluator.cpp`
- `src/core/ProjectController.cpp`
- `tests/tests_main.cpp`

### 2) Встроенная консоль позволяла выполнять `bash` по префиксу `!`
Симптом: команда `!...` напрямую запускала shell. Это серьёзная поверхность атаки.

Исправлено:
- shell-команды отключены по умолчанию;
- для явного включения нужен `PLOTAPP_ENABLE_SHELL=1`.

Файлы:
- `src/commands/CommandDispatcher.cpp`
- `src/ui/MainWindow.cpp`
- `docs/COMMAND_HELP.txt`
- `docs/CLI_COMMANDS.md`
- `README.md`

### 3) Загрузка проекта принимала `NaN`/`Inf`
Симптом: `.plotapp` с `POINT=nan,1` или нечисловым viewport загружался и вносил битое состояние в проект.

Исправлено:
- загрузчик проекта теперь отвергает не-конечные числовые значения;
- кастомный viewport при загрузке нормализуется: обратные границы меняются местами, нулевая ширина/высота расширяется.

Файлы:
- `src/serialization/ProjectSerializer.cpp`
- `tests/tests_main.cpp`

### 4) `NaN`/`Inf` могли проникать через импорт, параметры плагинов и ручной ввод точек
Симптом: не-конечные значения могли попадать в проект обходными путями.

Исправлено:
- общий числовой парсер теперь отвергает `NaN`/`Inf`;
- ручное добавление точки требует конечных координат;
- редактор точек в GUI игнорирует не-конечные значения.

Файлы:
- `src/core/TextUtil.cpp`
- `src/core/PluginManager.cpp`
- `src/core/ProjectController.cpp`
- `src/ui/PointsEditorDialog.cpp`
- `tests/tests_main.cpp`

### 5) Рендер и вычисление границ учитывали битые/скрытые точки недостаточно жёстко
Симптом: невалидные значения и скрытые точки могли искажать bounds и рендер.

Исправлено:
- экспорт SVG и canvas теперь игнорируют скрытые и не-конечные точки;
- границы viewport дополнительно санитизируются перед рендером.

Файлы:
- `src/render/SvgRenderer.cpp`
- `src/ui/PlotCanvasWidget.cpp`

## Что в итоге подтверждено

Подтверждено в контейнере:
- сборка core/CLI/plugins проходит;
- тесты проходят;
- ASan/UBSan тесты проходят;
- сценарии с `1/0`, `NaN` в проекте и `!команда` больше не ведут к прежнему опасному поведению.

## Что не завершено полностью

### GUI Qt6 не был полностью собран в контейнере
Причина: в контейнере нет Qt6 development packages.

Что проверять у себя:
- полноценную GUI-сборку;
- открытие/сохранение проекта из UI;
- редактор точек;
- масштабирование/панорамирование графика;
- поведение подсказки и сообщений в командной строке GUI.

Где смотреть:
- `CMakeLists.txt`
- `src/ui/MainWindow.cpp`
- `src/ui/PlotCanvasWidget.cpp`
- `src/ui/PointsEditorDialog.cpp`

### Архитектурная зона риска, не переписывал в этом проходе
Система плагинов по-прежнему исполняет нативные `.so`-модули. Это ожидаемое поведение, но это доверенная граница, а не sandbox.

Если захотите дальше усиливать безопасность, смотреть сюда:
- `src/core/PluginManager.cpp`
- политику поставки/подписывания плагинов
- отдельный sandbox/изолированный процесс для плагинов
