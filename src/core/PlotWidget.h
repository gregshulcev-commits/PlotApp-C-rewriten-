#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QFont>
#include <QList>
#include <QPen>
#include <QWidget>

// Предварительные объявления
class PlotLayer;
class DataManager;

/**
 * @class PlotWidget
 * @brief Виджет для отображения координатной сетки и графиков
 *
 * PlotWidget отвечает за:
 * - Отрисовку осей координат и сетки
 * - Отображение всех слоев (PlotLayer)
 * - Масштабирование и панорамирование (в будущем)
 * - Подписи осей и заголовок графика
 */
class PlotWidget : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Конструктор PlotWidget
   * @param dataManager Указатель на DataManager для получения слоев
   * @param parent Родительский виджет
   */
  explicit PlotWidget(DataManager *dataManager, QWidget *parent = nullptr);
  ~PlotWidget();

  /**
   * @brief Устанавливает диапазон осей
   * @param xMin Минимальное значение X
   * @param xMax Максимальное значение X
   * @param yMin Минимальное значение Y
   * @param yMax Максимальное значение Y
   */
  void updateAxisRange();

  /**
   * @brief Устанавливает подписи осей
   * @param xLabel Подпись оси X
   * @param yLabel Подпись оси Y
   */
  void setAxisLabels(const QString &xLabel, const QString &yLabel);

  /**
   * @brief Устанавливает заголовок графика
   * @param title Заголовок
   */
  void setTitle(const QString &title);

protected:
  /**
   * @brief Отрисовка виджета
   */
  void paintEvent(QPaintEvent *event) override;

  /**
   * @brief Обработка изменения размера
   */
  void resizeEvent(QResizeEvent *event) override;

private slots:
  /**
   * @brief Слот для обработки нового слоя
   * @param layer Указатель на созданный слой
   */
  void onLayerCreated(PlotLayer *layer);

  /**
   * @brief Слот для обработки удаления слоя
   * @param layerName Имя удаленного слоя
   */
  void onLayerRemoved(const QString &layerName);

  /**
   * @brief Слот для обработки изменений данных
   */
  void onDataChanged();

private:
  /**
   * @brief Вычисление шага сетки
   * @param range диапазон значений
   * @return Шаг (1, 2, 5 * 10^n)
   */
  double calculateStep(double range) const;

  /**
   * @brief Отрисовка координатной сетки
   * @param painter QPainter для отрисовки
   * @param xStep шаг по оси x
   * @param yStep шаг по оси y
   */
  void drawGrid(QPainter &painter, double xStep, double yStep);

  /*
   * @brief Отрисовка значений пересечений сетки и осей
   * @param painter QPainter для отрисовки
   * @param xAxisY Положение оси x относительно Y
   * @param yAxisX Положение оси y относительно X
   * @param xStep шаг по оси x
   * @param yStep шаг по оси y
   */
  void drawAxisLabels(QPainter &painter, double xAxisY, double yAxisX,
                      double xStep, double yStep);

  /*
   * @brief Отрисовка стрелок на осях
   * @param painter Qpainter для отрисовки
   * @param xEnd координата по x
   * @param yEnd координата по y
   */
  void drawAxisArrows(QPainter &painter, const QPoint &xEnd,
                      const QPoint &yEnd);

  /**
   * @brief Отрисовка заголовка и названий осей
   * @param painter QPainter для отрисовки
   */
  void drawLabels(QPainter &painter);

  /**
   * @brief Отрисовка осей координат
   * @param painter QPainter для отрисовки
   */
  void drawAxes(QPainter &painter);

  /**
   * @brief Отрисовка всех слоев
   * @param painter QPainter для отрисовки
   */
  void drawLayers(QPainter &painter);

  /**
   * @brief Преобразование координат графика в экранные координаты
   * @param x Координата X в системе графика
   * @param y Координата Y в системе графика
   * @return QPoint Экранные координаты
   */
  QPoint graphToScreen(double x, double y) const;

  /**
   * @brief Обновление трансформации координат при изменении размера
   */
  void updateCoordinateTransform();

  DataManager *m_dataManager;  ///< Указатель на менеджер данных
  QList<PlotLayer *> m_layers; ///< Список слоев для отображения

  // Настройки осей
  double m_xMin, m_xMax;      ///< Диапазон оси X
  double m_yMin, m_yMax;      ///< Диапазон оси Y
  QString m_xLabel, m_yLabel; ///< Подписи осей
  QString m_title;            ///< Заголовок графика

  // Настройки отображения
  QPen m_gridPen;    ///< Перо для сетки
  QPen m_axisPen;    ///< Перо для осей
  QFont m_labelFont; ///< Шрифт для подписей

  // Трансформация координат
  double m_scaleX, m_scaleY;   ///< Масштаб по осям
  double m_offsetX, m_offsetY; ///< Смещение по осям
  QRect m_plotArea;            ///< Область для рисования графика
};

#endif // PLOTWIDGET_H
