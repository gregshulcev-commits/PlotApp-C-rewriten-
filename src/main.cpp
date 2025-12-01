#include <QApplication>
#include <QMainWindow>
#include "core/DataManager.h"
#include "core/PlotWidget.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  // Создаем менеджер данных и виджет графика
  DataManager dataManager;
  PlotWidget plotWidget(&dataManager);

  // Настраиваем заголовок и подписи
  plotWidget.setTitle("Тестовый график");
  plotWidget.setAxisLabels("Время, с", "Напряжение, В");

  // Создаем главное окно
  QMainWindow window;
  window.setCentralWidget(&plotWidget);
  window.setWindowTitle("PlotApp - Тест отрисовки");
  window.resize(800, 600);
  window.show();

  qDebug() << "=== PlotApp запущен ===";
  qDebug() << "Проверяем отрисовку осей, сетки и подпись";

  return app.exec(); 

}
