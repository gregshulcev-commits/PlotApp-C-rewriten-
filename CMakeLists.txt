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
