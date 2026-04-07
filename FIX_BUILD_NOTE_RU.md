# Исправление ошибки сборки GUI

Исправлена ошибка:
- `src/app/main.cpp` подключал `#include "ui/MainWindow.h"`, но этот путь не попадал в include-path цели `plotapp`.

Что изменено:
1. В `src/app/main.cpp` заменён include на `#include "MainWindow.h"`.
2. В `CMakeLists.txt` для цели `plotapp` добавлены include-пути:
   - `src`
   - `src/ui`
3. Удалено одно неиспользуемое предупреждение в `ProjectSerializer.cpp`.

Если после этого появятся новые ошибки Qt-части, пришли лог, и я добью следующий слой исправлений.
