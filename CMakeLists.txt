<<<<<<< HEAD
cmake_minimum_required(VERSION 3.16)
project(PlotApp VERSION 0.1.0 LANGUAGES CXX)

# ========== НАСТРОЙКА ОЧИСТКИ ==========
# Устанавливаем выходную директорию для бинарников
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Очистка старых файлов при переконфигурации
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY CLEAN_NO_CUSTOM 1)

# ========== НАСТРОЙКА QT ==========
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# ========== КОНФИГУРАЦИЯ ПРОЕКТА ==========
add_executable(PlotApp
    src/main.cpp
    src/core/DataManager.cpp
    src/core/PlotWidget.cpp
)

target_link_libraries(PlotApp Qt6::Core Qt6::Widgets)
target_compile_features(PlotApp PRIVATE cxx_std_17)

set_target_properties(PlotApp PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

target_include_directories(PlotApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/features
)

# ========== КОМАНДЫ ДЛЯ ОЧИСТКИ ==========
# Кастомная команда для полной очистки
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory bin
    COMMAND ${CMAKE_COMMAND} -E remove_directory lib
    COMMENT "Полная очистка build директории"
)

# ========== ПОСЛЕКОМПИЛЯЦИОННАЯ ОЧИСТКА ==========
# Автоматически удаляем промежуточные файлы (опционально)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET PlotApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Сборка завершена. Файлы в: ${CMAKE_BINARY_DIR}/bin"
    )
endif()
=======
cmake_minimum_required(VERSION 3.20)
project(plotapp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PLOTAPP_BUILD_GUI "Build Qt6 desktop UI" ON)
option(PLOTAPP_BUILD_TESTS "Build tests" ON)

include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Threads REQUIRED)
find_package(MiniZip REQUIRED)
find_package(Qt6 COMPONENTS Widgets Gui Core QUIET)

add_library(plotapp_core
    src/core/Project.cpp
    src/core/TextUtil.cpp
    src/core/FormulaEvaluator.cpp
    src/core/PluginManager.cpp
    src/core/ProjectController.cpp
    src/core/LayerSampler.cpp
    src/importers/DelimitedTextImporter.cpp
    src/importers/XlsxImporter.cpp
    src/serialization/ProjectSerializer.cpp
    src/render/SvgRenderer.cpp
    src/commands/CommandDispatcher.cpp
)

target_include_directories(plotapp_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(plotapp_core
    PUBLIC
        Threads::Threads
        MiniZip::MiniZip
        dl
)

target_compile_options(plotapp_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(plotapp-cli src/cli/main.cpp)
target_link_libraries(plotapp-cli PRIVATE plotapp_core)
target_compile_options(plotapp-cli PRIVATE -Wall -Wextra -Wpedantic)

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

plotapp_add_plugin(plotapp_plugin_linear_fit plugins/linear_fit/linear_fit_plugin.cpp plotapp_linear_fit)
plotapp_add_plugin(plotapp_plugin_moving_average plugins/moving_average/moving_average_plugin.cpp plotapp_moving_average)
plotapp_add_plugin(plotapp_plugin_local_extrema plugins/local_extrema/local_extrema_plugin.cpp plotapp_local_extrema)
plotapp_add_plugin(plotapp_plugin_newton_deg2 plugins/newton_deg2/newton_deg2_plugin.cpp plotapp_newton_deg2)
plotapp_add_plugin(plotapp_plugin_newton_deg4 plugins/newton_deg4/newton_deg4_plugin.cpp plotapp_newton_deg4)
plotapp_add_plugin(plotapp_plugin_newton_deg5 plugins/newton_deg5/newton_deg5_plugin.cpp plotapp_newton_deg5)
plotapp_add_plugin(plotapp_plugin_newton_polynomial plugins/newton_polynomial/newton_polynomial_plugin.cpp plotapp_newton_polynomial)
plotapp_add_plugin(plotapp_plugin_smooth_curve plugins/smooth_curve/smooth_curve_plugin.cpp plotapp_smooth_curve)
plotapp_add_plugin(plotapp_plugin_error_bars plugins/error_bars/error_bars_plugin.cpp plotapp_error_bars)

if(PLOTAPP_BUILD_GUI)
    if(Qt6_FOUND)
        qt_standard_project_setup()
        add_executable(plotapp
            src/app/main.cpp
            src/ui/MainWindow.cpp
            src/ui/PlotCanvasWidget.cpp
            src/ui/ImportDialog.cpp
            src/ui/LayerPropertiesDialog.cpp
            src/ui/PointsEditorDialog.cpp
            src/ui/PluginRunDialog.cpp
            src/ui/FormulaLayerDialog.cpp
            src/ui/SettingsDialog.cpp
            src/ui/TextEntryDialog.cpp
        )
        target_include_directories(plotapp PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
        )
        target_link_libraries(plotapp PRIVATE plotapp_core Qt6::Core Qt6::Gui Qt6::Widgets)
        target_compile_options(plotapp PRIVATE -Wall -Wextra -Wpedantic)
    else()
        message(WARNING "Qt6 not found. GUI target 'plotapp' will not be built in this environment.")
    endif()
endif()

if(PLOTAPP_BUILD_TESTS)
    enable_testing()
    add_executable(plotapp_tests
        tests/tests_main.cpp
    )
    target_link_libraries(plotapp_tests PRIVATE plotapp_core)
    target_compile_definitions(plotapp_tests PRIVATE
        PLOTAPP_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        PLOTAPP_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
    )
    add_test(NAME plotapp_tests COMMAND plotapp_tests)
    set_tests_properties(plotapp_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()

install(TARGETS plotapp_core plotapp-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(TARGET plotapp)
    install(TARGETS plotapp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(TARGETS
    plotapp_plugin_linear_fit
    plotapp_plugin_moving_average
    plotapp_plugin_local_extrema
    plotapp_plugin_newton_deg2
    plotapp_plugin_newton_deg4
    plotapp_plugin_newton_deg5
    plotapp_plugin_newton_polynomial
    plotapp_plugin_smooth_curve
    plotapp_plugin_error_bars
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/plotapp/plugins
)

install(DIRECTORY include/plotapp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY docs DESTINATION ${CMAKE_INSTALL_DATADIR}/plotapp)
install(DIRECTORY examples DESTINATION ${CMAKE_INSTALL_DATADIR}/plotapp)
install(FILES VERSION.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/plotapp)
install(PROGRAMS tools/desktop_manager.sh DESTINATION ${CMAKE_INSTALL_DATADIR}/plotapp/system)
install(FILES packaging/linux/plotapp.desktop.in DESTINATION ${CMAKE_INSTALL_DATADIR}/plotapp/system)
install(FILES assets/icons/plotapp.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
>>>>>>> d851259 (v1:Install & Update manager was initialised)
