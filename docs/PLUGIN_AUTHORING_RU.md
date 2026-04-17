# Подробная инструкция: как создать новый плагин для PlotApp

Этот документ рассчитан на разработчика, который **не хочет разбираться во всем ядре приложения**, но хочет быстро и безопасно добавить новый плагин.

Главная идея архитектуры PlotApp:
- ядро приложения и UI живут отдельно;
- плагины собираются в **отдельные `.so`-библиотеки**;
- чтобы добавить новый плагин, **не нужно встраивать алгоритм в ядро**;
- плагин работает только с тем слоем и тем набором точек, который ему передало приложение.

То есть нормальный путь такой:
1. создать отдельную папку `plugins/<my_plugin>/`;
2. написать файл `..._plugin.cpp`;
3. подключить `plotapp/PluginApi.h`;
4. добавить новый target в `CMakeLists.txt`;
5. собрать проект;
6. положить `.so` в каталог плагинов;
7. нажать **Reload plugins** в UI или перезапустить приложение.

---

## 1. Что должен уметь плагин

Плагин в PlotApp — это отдельная динамическая библиотека, которая получает:
- один исходный слой;
- массив точек `x, y`;
- строку параметров `key=value;key=value`.

И возвращает:
- новый массив точек;
- рекомендуемое имя нового слоя;
- необязательное предупреждение.

Плагин **не должен**:
- напрямую работать с Qt UI;
- читать внутренние структуры проекта;
- редактировать другие слои;
- рассчитывать, что он применится к нескольким слоям сразу.

Плагин **должен** считать, что он работает только с одним входным слоем.

---

## 2. Как данные попадают в плагин

Очень важный момент: плагин не знает, выделил пользователь весь слой или только часть точек.

Это делает ядро приложения:
- пользователь выбирает **один** слой;
- пользователь может выделить все точки слоя или прямоугольником выбрать только часть;
- `ProjectController` строит временное представление слоя;
- в плагин попадают **уже отфильтрованные** точки.

Для плагина это выглядит просто как:
- `request->source_layer.points`
- `request->source_layer.point_count`

Поэтому в самом плагине не нужно реализовывать логику выделения. Он всегда работает с тем, что ему уже дали.

---

## 3. Минимальный ABI плагина

Каждый плагин обязан экспортировать **три** функции:

1. `plotapp_get_metadata`
2. `plotapp_run`
3. `plotapp_free_result`

Они объявлены в:

```cpp
#include "plotapp/PluginApi.h"
```

### 3.1. `plotapp_get_metadata`
Возвращает описание плагина:
- версия API;
- id плагина;
- отображаемое имя;
- описание;
- параметры по умолчанию.

### 3.2. `plotapp_run`
Основная функция. Получает входной слой и строку параметров, строит результат.

### 3.3. `plotapp_free_result`
Освобождает память, которую плагин сам выделил внутри `plotapp_run`.

Это критично. Если забыть освобождение памяти, будут утечки.

---

## 4. Структура папки плагина

Рекомендуемая структура:

```text
plugins/
  my_plugin/
    my_plugin.cpp
```

Если нужен общий код, можно:
- сделать небольшой helper прямо в этом `.cpp`;
- либо вынести общие функции в `plugins/common/`.

Но для первого плагина лучше начать с **одного файла**.

---

## 5. Самый простой шаблон плагина

Ниже — минимальный рабочий шаблон.

```cpp
#include "plotapp/PluginApi.h"

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

namespace {

char* dupString(const std::string& value) {
    char* out = static_cast<char*>(std::malloc(value.size() + 1));
    if (!out) return nullptr;
    std::memcpy(out, value.c_str(), value.size() + 1);
    return out;
}

} // namespace

extern "C" PlotAppPluginMetadata plotapp_get_metadata() {
    return PlotAppPluginMetadata{
        PLOTAPP_PLUGIN_API_VERSION,
        "my_plugin",
        "My plugin",
        "Описание плагина",
        "samples=128"
    };
}

extern "C" int plotapp_run(const PlotAppPluginRequest* request, PlotAppPluginResult* result) {
    if (!request || !result) return 1;
    if (request->source_layer.point_count == 0) return 2;

    result->point_count = request->source_layer.point_count;
    result->points = static_cast<PlotAppPoint*>(
        std::malloc(sizeof(PlotAppPoint) * result->point_count)
    );
    if (!result->points) return 3;

    for (std::size_t i = 0; i < result->point_count; ++i) {
        result->points[i] = request->source_layer.points[i];
    }

    std::ostringstream name;
    name << request->source_layer.layer_name << " / my plugin";
    result->suggested_layer_name = dupString(name.str());
    result->warning_message = nullptr;
    return 0;
}

extern "C" void plotapp_free_result(PlotAppPluginResult* result) {
    if (!result) return;
    std::free(result->points);
    std::free(result->suggested_layer_name);
    std::free(result->warning_message);
    result->points = nullptr;
    result->suggested_layer_name = nullptr;
    result->warning_message = nullptr;
    result->point_count = 0;
}
```

Этот плагин ничего не считает — он просто возвращает исходные точки. Но это лучший старт: сначала добиться загрузки и запуска, потом добавлять математику.

---

## 6. Что означают структуры из `PluginApi.h`

### `PlotAppPoint`
Обычная точка:

```cpp
struct PlotAppPoint {
    double x;
    double y;
};
```

### `PlotAppLayerView`
То, что плагин видит как входной слой:

```cpp
struct PlotAppLayerView {
    const char* layer_id;
    const char* layer_name;
    const PlotAppPoint* points;
    size_t point_count;
};
```

Важно:
- `points` — это только **чтение**;
- сохранять указатель на `points` после завершения `plotapp_run` нельзя.

### `PlotAppPluginRequest`
Запрос плагину:

```cpp
struct PlotAppPluginRequest {
    PlotAppLayerView source_layer;
    const char* params;
};
```

### `PlotAppPluginResult`
Результат работы плагина:

```cpp
struct PlotAppPluginResult {
    PlotAppPoint* points;
    size_t point_count;
    char* suggested_layer_name;
    char* warning_message;
};
```

---

## 7. Как правильно придумать `plugin_id`

`plugin_id` должен быть:
- коротким;
- стабильным;
- уникальным;
- в lowercase;
- лучше с `_`, а не с пробелами.

Примеры:
- `linear_fit`
- `moving_average`
- `error_bars`
- `my_custom_filter`

Почему это важно:
- `plugin_id` сохраняется в проектном файле;
- по нему приложение потом пытается заново пересчитать производный слой;
- если поменять `plugin_id`, старые проекты перестанут корректно пересчитываться.

То есть после выпуска плагина **лучше не переименовывать id**.

---

## 8. Как работать с параметрами

Параметры передаются строкой:

```text
key=value;key=value;key=value
```

Например:
- `samples=128`
- `degree=3;samples=256`
- `window=5;tolerance=0.1`
- `show_axis_intersections=1`

### Рекомендации
- использовать lowercase-имена;
- разделять слова через `_`;
- иметь безопасные значения по умолчанию;
- игнорировать неизвестные параметры, если это возможно;
- не падать только потому, что опциональный параметр не пришел.

### Пример разбора числа

```cpp
int parseSamples(const char* params) {
    if (!params || std::strlen(params) == 0) return 128;
    std::string text(params);
    std::size_t pos = text.find("samples=");
    if (pos == std::string::npos) return 128;
    return std::max(2, std::stoi(text.substr(pos + 8)));
}
```

### Пример boolean-параметра

```cpp
bool parseBoolParam(const char* params, const char* key, bool fallback) {
    if (!params || !key) return fallback;
    std::string text(params);
    std::string pattern = std::string(key) + "=";
    std::size_t pos = text.find(pattern);
    if (pos == std::string::npos) return fallback;

    std::string value = text.substr(pos + pattern.size());
    std::size_t end = value.find(';');
    if (end != std::string::npos) value = value.substr(0, end);

    if (value == "1" || value == "true" || value == "yes" || value == "on") return true;
    if (value == "0" || value == "false" || value == "no" || value == "off") return false;
    return fallback;
}
```

---

## 9. Какой алгоритм лучше возвращать

Зависит от типа результата.

### 9.1. Если это линия / сглаживание / аппроксимация
Возвращайте точки в порядке роста `x`, чтобы ядро могло соединить их линией.

Примеры:
- линейная аппроксимация;
- полиномиальная интерполяция;
- сглаженная кривая.

### 9.2. Если это только специальные точки
Возвращайте только важные точки.

Примеры:
- локальные минимумы/максимумы;
- пики;
- точки пересечения;
- маркеры особых событий.

### 9.3. Не раздувайте результат без необходимости
Если исходных точек 500, а вы возвращаете 5 000 000 точек без реальной необходимости, это плохо:
- замедлит UI;
- утяжелит проектный файл;
- ухудшит экспорт.

Общее правило: чем проще достаточно представить результат, тем лучше.

---

## 10. Правильная работа с памятью

Это самый частый источник багов.

### Нельзя
- возвращать указатель на локальный массив;
- возвращать `std::string::c_str()` как результат;
- выделить через `new`, а освобождать через `free`;
- забыть обнулить указатели в `plotapp_free_result`.

### Нужно
- выделять `result->points` через `malloc`;
- выделять строки через `malloc` или helper `dupString`;
- освобождать всё в `plotapp_free_result`.

### Правильный паттерн

```cpp
result->points = static_cast<PlotAppPoint*>(
    std::malloc(sizeof(PlotAppPoint) * result->point_count)
);
```

И потом:

```cpp
std::free(result->points);
```

Одним стилем. Без смешивания `new/delete` и `malloc/free`.

---

## 11. Коды возврата

`plotapp_run` возвращает `int`.

Принято:
- `0` — успех;
- любое ненулевое значение — ошибка.

Пример:
- `1` — плохие входные аргументы;
- `2` — недостаточно точек;
- `3` — не удалось выделить память;
- `4` — вырожденный случай (например, деление на ноль в формуле алгоритма).

Главное — быть последовательным внутри плагина.

---

## 12. Как добавить плагин в сборку

В `CMakeLists.txt` уже есть helper:

```cmake
function(plotapp_add_plugin target_name source_file output_name)
    add_library(${target_name} MODULE ${source_file})
    target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    set_target_properties(${target_name} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${output_name}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    )
endfunction()
```

Чтобы зарегистрировать новый плагин, добавьте строку по аналогии:

```cmake
plotapp_add_plugin(plotapp_plugin_my_plugin plugins/my_plugin/my_plugin.cpp plotapp_my_plugin)
```

Три имени тут значат:
- `plotapp_plugin_my_plugin` — внутреннее имя target в CMake;
- `plugins/my_plugin/my_plugin.cpp` — файл исходника;
- `plotapp_my_plugin` — имя итоговой `.so`.

Важно:
- итоговая библиотека должна начинаться с `plotapp_`;
- расширение `.so` будет добавлено автоматически.

То есть на выходе получится примерно:

```text
build/plugins/plotapp_my_plugin.so
```

---

## 13. Как ядро находит плагины

Поиск идет по plugin directories. `PluginManager` делает discovery и загружает `.so`-файлы, у которых:
- имя начинается с `plotapp_`;
- расширение `.so`;
- есть все обязательные экспортируемые функции.

Практически это значит:
- собрали проект;
- получили `.so`;
- положили в каталог плагинов;
- нажали **Reload plugins**.

---

## 14. Быстрый сценарий создания нового плагина

### Шаг 1. Создайте папку

```text
plugins/my_plugin/
```

### Шаг 2. Добавьте `my_plugin.cpp`
Скопируйте минимальный шаблон и переименуйте id/название.

### Шаг 3. Подключите target в `CMakeLists.txt`

```cmake
plotapp_add_plugin(plotapp_plugin_my_plugin plugins/my_plugin/my_plugin.cpp plotapp_my_plugin)
```

### Шаг 4. Соберите проект

```bash
cmake -S . -B build
cmake --build build
```

### Шаг 5. Убедитесь, что файл собрался

```text
build/plugins/plotapp_my_plugin.so
```

### Шаг 6. Запустите приложение и нажмите Reload plugins
После этого плагин должен появиться в списке.

---

## 15. Как протестировать плагин без GUI

Сначала удобно тестировать через CLI или через unit tests.

### CLI-подход
1. импортировать слой;
2. применить плагин;
3. посмотреть список слоев или экспорт.

### Unit-test-подход
Можно добавить тест в `tests/tests_main.cpp` по аналогии с уже существующими тестами для:
- `linear_fit`
- `smooth_curve`
- `error_bars`
- `local_extrema`

Минимум, что хорошо проверять:
- плагин обнаруживается (`discover()`);
- плагин запускается;
- возвращает ожидаемое число точек;
- не падает на граничных условиях;
- корректно отрабатывает параметры.

---

## 16. Как сделать, чтобы в UI появился дополнительный контрол для плагина

Иногда одного `params` мало, и хочется checkbox/spinbox в диалоге запуска плагина.

Текущий путь в проекте такой:
1. добавить новый опциональный параметр в сам плагин;
2. обновить `default_params` в `plotapp_get_metadata`;
3. в `src/ui/PluginRunDialog.cpp` добавить UI-контрол для конкретного `plugin_id`;
4. при подтверждении диалога записывать этот выбор обратно в параметрическую строку.

Так сейчас сделано, например, для:
- `newton_polynomial` → степень полинома;
- `error_bars` → imported error column;
- `linear_fit` → `show_axis_intersections=1`.

Это хороший путь, потому что:
- ABI не меняется;
- старые проекты продолжают работать;
- логика плагина остается внутри плагина.

---

## 17. Частые ошибки при разработке плагина

### Ошибка 1. Менять ядро вместо создания `.so`
Неправильно:
- добавить алгоритм в `src/core/...` и звать его напрямую.

Правильно:
- отдельный плагин в `plugins/<name>/...`.

### Ошибка 2. Нестабильный `plugin_id`
Если вы сегодня назвали плагин `fit_test`, а завтра `fit_test_v2`, старые проекты могут перестать пересчитываться.

### Ошибка 3. Возврат указателей на временные данные
Например:

```cpp
std::string name = "abc";
result->suggested_layer_name = const_cast<char*>(name.c_str());
```

Так делать нельзя. После выхода из функции память недействительна.

### Ошибка 4. Нет защиты от плохого ввода
Плагин должен спокойно переживать:
- `nullptr` в request/result;
- слишком мало точек;
- вырожденные случаи.

### Ошибка 5. Нет параметров по умолчанию
Если без параметров плагин ведет себя странно, UI будет неудобен. Лучше всегда иметь безопасный default.

### Ошибка 6. Плагин генерирует слишком много точек
Это особенно вредно для тяжелых кривых и экспорта.

---

## 18. Практический пример: линейная аппроксимация

Плагин `linear_fit` в репозитории — хороший реальный пример.

Что он показывает:
- как собрать коэффициенты прямой по МНК;
- как создать `suggested_layer_name`;
- как принимать параметр `samples`;
- как добавить новый параметр `show_axis_intersections` без изменения ABI.

Если нужен свой плагин похожего класса (аппроксимация/интерполяция/сглаживание), ориентируйтесь на него как на базовый шаблон.

---

## 19. Когда нужен новый API, а когда нет

В большинстве случаев новый API **не нужен**.

Если вам нужно:
- больше параметров;
- дополнительные опции в UI;
- включать/выключать режимы;
- менять плотность дискретизации;

— это почти всегда можно сделать через обычную строку `params`.

Новый ABI стоит обсуждать только если плагину реально нужны данные, которых сейчас нет в `PlotAppLayerView`.

Пока что большинство задач закрывается без изменения ABI.

---

## 20. Рекомендованный шаблон работы

Если вы хотите создать новый плагин «без погружения в ядро», лучший порядок такой:

1. Скопировать существующий простой плагин (`linear_fit` или `moving_average`).
2. Переименовать:
   - `plugin_id`
   - display name
   - suggested layer name
3. Заменить математическую часть на свою.
4. Добавить 1-2 параметра в `default_params`.
5. Проверить `.so`-сборку.
6. Проверить обнаружение плагина.
7. Проверить работу на маленьком тестовом наборе точек.
8. Только потом добавлять UI-специфичные контролы в `PluginRunDialog`.

Это самый безопасный путь.

---

## 21. Чек-лист перед коммитом

Перед тем как считать плагин готовым, пройдите чек-лист:

- [ ] плагин живет в отдельной папке `plugins/<name>/`
- [ ] собирается в отдельную `.so`
- [ ] имеет стабильный `plugin_id`
- [ ] экспортирует все 3 обязательные функции
- [ ] не зависит от Qt UI
- [ ] корректно работает на одном входном слое
- [ ] корректно работает на выделенном подмножестве точек
- [ ] имеет безопасные параметры по умолчанию
- [ ] освобождает всю память в `plotapp_free_result`
- [ ] не меняет ядро без реальной необходимости
- [ ] имеет хотя бы минимальный тестовый сценарий

---

## 22. Куда смотреть в коде репозитория

Полезные ориентиры:
- ABI: `include/plotapp/PluginApi.h`
- загрузка/запуск: `src/core/PluginManager.cpp`
- подготовка выбранного подмножества точек: `src/core/ProjectController.cpp`
- диалог запуска плагинов: `src/ui/PluginRunDialog.cpp`
- примеры плагинов:
  - `plugins/linear_fit/linear_fit_plugin.cpp`
  - `plugins/moving_average/moving_average_plugin.cpp`
  - `plugins/local_extrema/local_extrema_plugin.cpp`
  - `plugins/error_bars/error_bars_plugin.cpp`

---

## 23. Итог

Чтобы создать новый плагин для PlotApp, **не нужно переписывать ядро**.

Нормальный путь:
- отдельный `.cpp`;
- отдельная `.so`;
- 3 обязательные функции ABI;
- параметры через строку;
- сборка через `plotapp_add_plugin(...)`;
- при необходимости — маленькое расширение `PluginRunDialog`, но без изменения ABI.

Это и есть рекомендуемый способ расширять систему дальше.
