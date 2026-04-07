<<<<<<< HEAD
# PlotApp - Система построения научных графиков

## ОПИСАНИЕ ПРОЕКТА
Интерактивное приложение для построения научных графиков с расширяемой архитектурой.
Поддерживает базовое построение графиков, интерактивное взаимодействие и модульную систему дополнительных функций.

## СТРУКТУРА ПРОЕКТА

plot_app/
├── CMakeLists.txt                 # Основной файл сборки CMake
├── README.txt                     # Эта документация
├── src/                           # Исходный код
│   ├── main.cpp                   # Точка входа в приложение
│   ├── core/                      # ЯДРО ПРИЛОЖЕНИЯ
│   │   ├── PlotWidget.h/cpp       # Основной виджет графика
│   │   ├── DataManager.h/cpp      # Менеджер данных и слоев
│   │   └── interactions/          
│   │       └── ZoomPanInteraction.h/cpp  # Обработка масштабирования/перемещения (Пока не реализовано)
│   ├── ui/                        
│   │   └── MainWindow.h/cpp       # Главное окно приложения
│   └── utils/
│       └── MathUtils.h/cpp        # Вспомогательные математические функции
├── features/                      # ДОПОЛНИТЕЛЬНЫЙ ФУНКЦИОНАЛ
│   └── ManualPointInput.h/cpp     # Ручной ввод точек по координатам
└── resources/                     # РЕСУРСЫ
    └── icons/                     # Иконки приложения

## ДЕТАЛЬНОЕ ОПИСАНИЕ КОМПОНЕНТОВ

### ЯДРО (CORE)

#### 1. PlotWidget (PlotWidget.h/cpp)
- Основной виджет для отображения графиков
- Функциональность:
  * Оси координат с подписями
  * Сетка (основная и дополнительная)
  * Легенда графиков
  * Название графика
  * Прилипание осей к краям при перемещении
  * Базовое управление слоями (привязывает слои к сетке координат)

#### 2. DataManager (DataManager.h/cpp)  
- Центральный менеджер данных приложения
- Функциональность:
  * Хранение всех данных графиков
  * Управление жизненным циклом слоев
  * Координация между различными компонентами
  * Thread-safe операции для многопоточности
  * Сигналы для уведомления об изменениях данных
  * Сериализация/десериализация проектов

#### 3. ZoomPanInteraction (ZoomPanInteraction.h/cpp) (пока не реализовано)
- Обработчик пользовательского взаимодействия
- Функциональность:
  * Масштабирование колесом мыши
  * Перемещение графика перетаскиванием
  * Выделение областей

### ПОЛЬЗОВАТЕЛЬСКИЙ ИНТЕРФЕЙС (UI)

#### MainWindow (MainWindow.h/cpp)
- Главное окно приложения
- Содержит:
  * Панель инструментов с кнопками (работа с легендой, названиями осей и графика в целом и тд)
  * Основной виджет графика
  * Меню для доступа к функциям (работа с фичами)

### ДОПОЛНИТЕЛЬНЫЙ ФУНКЦИОНАЛ (FEATURES)

#### ManualPointInput (ManualPointInput.h/cpp)
- Модуль для ручного ввода точек пользователем
- Функциональность:
  * Диалоговое окно для ввода координат X,Y
  * Валидация вводимых данных
  * Добавление точек в существующий или новый слой (при введении точки в первый раз создаем слой, при обновлении слоя (работа со слоем) добавляем в существующий или изменяем в существующим и обновляем отображение слоя)

# Компонентная модель
Application (QApplication)
    ↓
MainWindow (UI координатор)
    ↓    
DataManager (ядро системы) ←→ FeatureRegistry (реестр фич)
    ↓
PlotWidget (контейнер слоев) ←→ Features (изолированные модули)
    ↓
PlotLayer (индивидуальные слои)

# Ключевые принципы
Изоляция фич - каждая фича независима и самодостаточна

DataManager как координатор - управляет слоями, но не содержит бизнес-логики

Единый источник истины - DataManager хранит все данные проекта

Асинхронность - каждая фича работает в своем потоке

Саморегистрация - фичи самостоятельно регистрируются в системе

# Стректуры данных
struct LayerInfo {
    QString name;
    QString type;
    QString featureName;    // Имя фичи, отвечающей за слой
    bool visible;
    QString serializedData; // Данные в формате фичи
    QDateTime created;
    QString description;
};

struct ProjectInfo {
    QString version;
    QDateTime saved;
    QList<LayerInfo> layers;
    PlotSettings plotSettings; // Настройки осей, сетки и т.д.
};

# Принцип работы слоев
Каждый слой - отдельный QWidget

Слои накладываются друг на друга в PlotWidget

Z-order определяется порядком создания

Каждый слой привязан к общим осям координат

Видимость управляется независимо

 # Инициализация приложения
1. Запуск MainWindow
2. Создание DataManager
3. Создание и регистрация всех фич:
   - GraphFeature → "GraphFeature"
   - ErrorBarsFeature → "ErrorBarsFeature" 
   - CurveFitFeature → "CurveFitFeature"
4. Инициализация PlotWidget
5. Показ главного окна 

# Создание нового графика (ручной режим)
1. Пользователь: "Добавить график"
2. MainWindow → DataManager::createLayer("График 1", "graph")
3. DataManager создает PlotLayer и emits layerCreated()
4. PlotWidget добавляет новый слой в компоновку
5. MainWindow → GraphFeature::openLayerDialog(layer)
6. Пользователь вводит данные в диалоге
7. GraphFeature::renderLayerAsync(layer, data)
8. Слой отрисовывается асинхронно
9. Сигнал завершения → обновление UI

# Редактирование существующего слоя
1. Пользователь: двойной клик на слое
2. PlotWidget → MainWindow::onLayerActivated(layer)
3. MainWindow определяет фичу по layer->featureName()
4. MainWindow → feature->openLayerDialog(layer)
5. Фича загружает текущие данные в диалог
6. Пользователь редактирует → подтверждает
7. Фича перерисовывает слой с новыми данными

# Сохранение проекта
1. Пользователь: "Сохранить проект"
2. MainWindow → DataManager::saveProject(filename)
3. DataManager для каждого слоя:
   - Сохраняет LayerInfo (имя, тип, видимость)
   - Вызывает feature->serializeData(layer->getData())
   - Записывает фичу и данные в файл
4. Формат файла:
   [Layer_1]
   name=График 1
   type=graph
   feature=GraphFeature
   data=1.0,2.5;3.0,4.2
   visible=true

# Загрузка проекта
1. Пользователь: "Открыть проект"
2. MainWindow → DataManager::loadProject(filename)
3. DataManager читает файл, для каждой секции:
   - Создает PlotLayer с параметрами из файла
   - Находит Feature* = getFeature(featureName)
   - Вызывает feature->restoreLayer(layer, serializedData)
4. Фича десериализует данные и асинхронно рисует слой
5. По завершении всех фич → обновление UI

# Модель потоков
Главный поток (UI):
  - Обработка пользовательского ввода
  - Управление диалогами
  - Обновление UI

Поток DataManager:
  - File I/O операции
  - Управление памятью слоев

Потоки фич (по одному на фичу):
  - Вычисления и рендеринг
  - Сериализация/десериализация

## НЕОБХОДИМЫЕ БИБЛИОТЕКИ И ИНСТРУМЕНТЫ

### ОСНОВНЫЕ БИБЛИОТЕКИ:

1. **Qt6 (Core, Widgets, Gui)**
   - Назначение: Базовый фреймворк для GUI приложения
   - Минимальная версия: Qt 6.2
   - Установка в Fedora: sudo dnf install qt6-qtbase-devel

2. **QCustomPlot**
   - Назначение: Библиотека для построения графиков
   - Версия: 2.1.1 или выше
   - Установка в Fedora: sudo dnf install qcustomplot-devel
   - Лицензия: GPLv3 (для коммерческого использования требуется лицензия)

### СИСТЕМНЫЕ ЗАВИСИМОСТИ:

3. **CMake** (версия 3.16 или выше)
   - Назначение: Система сборки проекта
   - Установка: sudo dnf install cmake

4. **C++ Compiler** с поддержкой C++17
   - Рекомендуется: g++ 10.0 или выше, clang 10.0 или выше
   - Установка: sudo dnf install gcc-c++

5. **Make** или **Ninja**
   - Назначение: Утилиты для сборки
   - Установка: sudo dnf install make ninja-build

## ИНСТРУКЦИЯ ПО СБОРКЕ

### Шаг 1: Установка зависимостей
```bash
sudo dnf install qt6-qtbase-devel qcustomplot-qt6-devel cmake gcc-c++ make

### Шаг 2: Клонирование и подготовка проекта
```bash
git cline <repository-url>
cd plot_app
mkdir build
cd build

### Шаг 3: Конфигурация и сборка
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

### Шаг 4: Запуск приложения
```bash
./PlotApp

## АРХИТЕКТУРА И ПРИНЦИПЫ РАЗРАБОТКИ
  ### Принципы взаимодействия компонентов:
    ### DataManager является центральным хабоm данных

    ### Все компоненты общаются через DataManager

    ### PlotWidget отображает данные из DataManager

    ###Дополнительные фичи работают через API DataManager


## Многопоточность:
  ### Архитектура поддерживает многопоточность через QThread

  ### DataManager thread-safe с использованием мьютексов

  ### UI операции всегда в главном потоке

## РАСШИРЕНИЕ ФУНКЦИОНАЛА
  ### Добавление новой функции:
    ### Создать файлы в папке features/ (например: NewFeature.h/cpp)

    ### Реализовать взаимодействие через DataManager

    ### Добавить UI элементы в MainWindow при необходимости

    ### Обновить CMakeLists.txt если нужно



## Формат файлов сохранения
[Project]
version=1.0
created=2024-01-01T10:00:00

[Layer_1]
name=График эксперимента
type=graph
feature=GraphFeature
data=0.0,1.1;1.0,2.3;2.0,3.7
visible=true
showInLegend=true

[Layer_2]
name=Погрешности
type=errors  
feature=ErrorBarsFeature
data=0.0,0.1;1.0,0.2;2.0,0.15
visible=true
showInLegend=false

Пользователь меняет данные 
    → layer->setData() 
    → сигнал dataChanged() 
    → PlotWidget::onDataChanged() 
    → update() 
    → paintEvent() 
    → drawLayers() 
    → feature->drawOnLayer()

void PlotWidget::drawLayers(QPainter &painter) {
    foreach (PlotLayer *layer, m_layers) {
        if (!layer->visible || !layer->attachedFeature) continue;
        
        // ВСЮ отрисовку делает фича
        layer->attachedFeature->drawOnLayer(painter, layer, *this);
    }
 


}
=======
# PlotApp Modular UI

PlotApp is a modular plotting application built around a C++17 core, a CLI/command-console workflow, and a Qt6 desktop UI wrapper.

This revision keeps the layered architecture intact and focuses on bug fixing, security hardening, and a few high-value functional improvements in the core/CLI path.

## What changed in this update

- fixed broken automated tests caused by fragile relative paths;
- hardened plugin discovery so the app no longer auto-loads arbitrary `.so` files from the current working directory;
- added safer default plugin discovery relative to the executable, plus optional `PLOTAPP_PLUGIN_DIR` override;
- added protection against duplicate plugin IDs and repeated `discover()` handle leaks;
- improved delimited-text import:
  - extensionless tabular files now work,
  - delimiter detection is more robust,
  - UTF-8 BOM is handled,
  - simple decimal-comma numeric cells are accepted,
  - obvious binary/image-like files are rejected as tabular data;
- added XLSX entry-size limits to reduce ZIP-bomb style risk;
- hardened project loading:
  - project header/version is validated,
  - malformed numeric fields report clearer errors,
  - save now uses a temp-file + rename flow instead of overwriting directly;
- fixed CLI `formula` parsing so a layer name can be passed without explicitly providing the sample count;
- formula layers that produce fewer than two finite points are now rejected instead of creating empty/broken layers;
- NaN/Inf numeric input is now rejected in project files, imports, plugin numeric params, and manual point insertion;
- improved SVG export:
  - title/labels/legend text are XML-escaped,
  - unsafe colors fall back to a safe default,
  - formula layers are rendered from formula metadata over the current visible viewport,
  - error bars are exported too;
- extended tests to cover extensionless import, invalid project rejection, SVG escaping/sanitization, plugin rediscovery, and formula-name parsing.

## Build

### Core + CLI only
```bash
cmake -S . -B build -G Ninja -DPLOTAPP_BUILD_GUI=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

### Full desktop UI on Fedora
```bash
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel minizip-ng-compat-devel
cmake -S . -B build -G Ninja
cmake --build build
```

## Run

Shell passthrough from the command console is now disabled by default. Enable it explicitly with `PLOTAPP_ENABLE_SHELL=1` only when you really need it.


CLI:
```bash
./build/plotapp-cli help
./build/plotapp-cli "import ./examples/sample_points.csv 0 1 raw"
./build/plotapp-cli "formula x^2 -2 2 parabola"
```

GUI:
```bash
./build/plotapp
```

## UI controls
- Mouse drag on plot area: pan viewport
- Mouse wheel: zoom both axes
- `Shift + wheel`: X-axis zoom only
- `Ctrl + wheel`: Y-axis zoom only
- Drag legend box: move that layer's legend
- Click title / X label / Y label: edit text inline
- `Ctrl + S`: save project

## Formula syntax
Examples:
- `sin(x)`
- `sin(x) + 0.2*x^2`
- `exp(-x^2)`
- `sqrt(abs(x))`
- `1e-3*x`

Supported:
- variable: `x`
- operators: `+ - * / ^`
- constants: `pi`, `e`
- functions: `sin cos tan asin acos atan sqrt abs exp log ln log10 floor ceil`

## LaTeX-like labels
The Qt canvas supports a lightweight LaTeX-like subset for title, axis labels and legend text:
- subscripts: `I_1`, `I_{out}`
- superscripts: `x^2`, `E^{max}`
- some Greek symbols: `\alpha`, `\beta`, `\mu`, `\sigma`, `\Delta`, `\Omega`, `\pi`


## Managed install, update and removal

PlotApp now includes a shell-first managed desktop install flow for Linux.

Install from the current source tree:
```bash
./install_app.sh --with-gui
```

Bind or inspect GitHub updates:
```bash
./update_app.sh --set-repo https://github.com/example/plotapp.git --branch main
./update_app.sh --check-only
```

Update to the newest revision:
```bash
./update_app.sh --yes
```

Remove the managed payload but keep runtime data:
```bash
./uninstall_app.sh --yes
```

Remove everything including runtime data/cache:
```bash
./uninstall_app.sh --purge-data --yes
```

The managed installer uses:
- `~/.local/share/plotapp-install/app/current` for the active payload,
- `~/.local/share/plotapp-install/app/previous` for rollback,
- `~/.local/share/plotapp-install/metadata/installation.manifest` as the shell-readable source of truth,
- `~/.local/share/plotapp-install/metadata/installation.json` as the diagnostic JSON mirror,
- `~/.local/bin/plotapp*` plus a GNOME desktop entry/icon for user-facing integration.

See `docs/MANAGED_INSTALL.md` for the full layout, manifest format, update workflow, and release discipline.

## Status note
The core, CLI, serializers, formula engine, importers, plugins and automated tests were rebuilt and verified in the container.
The Qt6 UI source is included and partially updated, but it was **not fully compiled in the container** because the container did not provide Qt6 development packages.
See `docs/STATUS_AND_GAPS.md`.
>>>>>>> d851259 (v1:Install & Update manager was initialised)
