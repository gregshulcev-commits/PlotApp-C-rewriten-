#include "PlotWidget.h"
#include "../features/Feature.h"
#include "DataManager.h"

#include <QDebug>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>

PlotWidget::PlotWidget(DataManager *dataManager, QWidget *parent)
    : QWidget(parent), m_dataManager(dataManager), m_xMin(-10.0), m_xMax(10.0),
      m_yMin(-10.0), m_yMax(10.0), m_xLabel("Ось X"), m_yLabel("Ось Y"),
      m_title("График") {

  // Настройка внешнего вида
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);

  // Настройка перьев
  m_gridPen = QPen(QColor(200, 200, 200), 1, Qt::DotLine);
  m_axisPen = QPen(Qt::black, 2, Qt::SolidLine);

  m_labelFont = QFont("Arial", 10);

  // Подключение сикналов от DataManager
  if (m_dataManager) {
    connect(m_dataManager, &DataManager::layerCreated, this,
            &PlotWidget::onLayerCreated);
    connect(m_dataManager, &DataManager::layerRemoved, this,
            &PlotWidget::onLayerRemoved);
    connect(m_dataManager, &DataManager::dataChanged, this,
            &PlotWidget::onDataChanged);
  }

  // Загрузка существующих слоев
  if (m_dataManager) {
    m_layers = m_dataManager->getAllLayers();
  }

  qDebug() << "PlotWidget: инициализирован";
}

PlotWidget::~PlotWidget() { qDebug() << "PlotWidget: уничтожен"; }

void PlotWidget::updateAxisRange() {
  if (m_layers.isEmpty()) {
    // Если слоев нет - стандартный диапазон
    m_xMin = -10.0;
    m_xMax = 10.0;
    m_yMin = -10.0;
    m_yMax = 10.0;

    updateCoordinateTransform();
    update();
    return;
  }

  // Ищем min/max среди всех видимых слоев
  double xMin = std::numeric_limits<double>::max();
  double xMax = std::numeric_limits<double>::lowest();
  double yMin = std::numeric_limits<double>::max();
  double yMax = std::numeric_limits<double>::lowest();

  foreach (PlotLayer *layer, m_layers) {
    if (!layer->visible)
      continue;

    for (double x : layer->data.xData) {
      xMin = qMin(xMin, x);
      xMax = qMax(xMax, x);
    }
    for (double y : layer->data.yData) {
      yMin = qMin(yMin, y);
      yMax = qMax(yMax, y);
    }
  }

  // Добавляем 10% запаса по краям
  double xMargin = (xMax - xMin) * 0.1;
  double yMargin = (yMax - yMin) * 0.1;

  m_xMin = xMin - xMargin;
  m_xMax = xMax + xMargin;
  m_yMin = yMin - yMargin;
  m_yMax = yMax + yMargin;

  updateCoordinateTransform();
  update();
  return;
}

void PlotWidget::setAxisLabels(const QString &xLabel, const QString &yLabel) {
  m_xLabel = xLabel;
  m_yLabel = yLabel;
  update(); // Перерисовать виджет
  qDebug() << "PlotWidget: подписи осей установлены - X:" << xLabel
           << "Y:" << yLabel;
}

void PlotWidget::setTitle(const QString &title) {
  m_title = title;
  update(); // Перерисовать виджет
  qDebug() << "PlotWidget: заголовок установлен -" << title;
}

void PlotWidget::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Очистка области
  painter.fillRect(rect(), palette().base());

  // Обновляем трасформацию координат
  updateCoordinateTransform();

  // Отрисовка компонентов
  drawAxes(painter);
  drawLayers(painter);
}

void PlotWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event); // Вызов базовой реализации

  // Обновляем трансформацию координат при изменении размера
  updateCoordinateTransform();

  qDebug() << "PlotWidget: размер изменен на" << size();
  update(); // Перерисовать с новыми размерами
}

void PlotWidget::onLayerCreated(PlotLayer *layer) {
  if (!layer)
    return;

  m_layers.append(layer);

  // Подключаемся к сигналам слоя для отслеживания изменения данных
  connect(layer, &PlotLayer::dataChanged, this, &PlotWidget::onDataChanged);

  updateAxisRange(); // Пересчитать диапазон осей
  update();          // Перерисовать

  qDebug() << "PlotWidget: добавлен слой" << layer->name;
}

void PlotWidget::onLayerRemoved(const QString &layerName) {
  // Ищем слой в списке по имени
  for (int i = 0; i < m_layers.size(); ++i) {
    if (m_layers[i]->name == layerName) {
      PlotLayer *layer = m_layers.takeAt(i); // Удаляем из списка

      // Отключаем соединения
      disconnect(layer, &PlotLayer::dataChanged, this,
                 &PlotWidget::onDataChanged);

      updateAxisRange();
      update();

      qDebug() << "PlotWidget: удален слой" << layerName;
      return;
    }
  }
  qWarning() << "PlotWidget: слой" << layerName << "не найден для удаления";
}

void PlotWidget::onDataChanged() {
  // Данные в одном из слоев изменились
  // Пересчитать диапазон осей под новые данные
  updateAxisRange();
  // Пересчитать график
  update();

  qDebug() << "PlotWidget: данные изменены, обновление отображения";
}

double PlotWidget::calculateStep(double range) const {
  if (range <= 0)
    return 1.0;

  double roughStep = range / 8.0; // 8 линий на диапазон
  double magnitude = pow(10, floor(log10(roughStep)));
  double normalized = roughStep / magnitude;

  if (normalized < 1.5)
    return magnitude;
  else if (normalized < 3.0)
    return 2 * magnitude;
  else
    return 5 * magnitude;
}

void PlotWidget::drawGrid(QPainter &painter, double xStep, double yStep) {
  painter.setPen(m_gridPen);

  // Вертикальные линии
  double startX = floor(m_xMin / xStep) * xStep;
  for (double x = startX; x <= m_xMax; x += xStep) {
    QPoint p1 = graphToScreen(x, m_yMin);
    QPoint p2 = graphToScreen(x, m_yMax);
    painter.drawLine(p1, p2);
  }

  // Горизонтальные линии
  double startY = floor(m_yMin / yStep) * yStep;
  for (double y = startY; y <= m_yMax; y += yStep) {
    QPoint p1 = graphToScreen(m_xMin, y);
    QPoint p2 = graphToScreen(m_xMax, y);
    painter.drawLine(p1, p2);
  }

  qDebug() << "Сетка: шаг X =" << xStep << "шаг Y =" << yStep;
}

void PlotWidget::drawAxisLabels(QPainter &painter, double xAxisY, double yAxisX,
                                double xStep, double yStep) {
  painter.setFont(m_labelFont);
  painter.setPen(Qt::black);

  // Подписи значений на оси X
  double startX = floor(m_xMin / xStep) * xStep;
  for (double x = startX; x <= m_xMax; x += xStep) {
    if (fabs(x) < 1e-10)
      x = 0;

    QPoint pos = graphToScreen(x, xAxisY);
    QString label = QString::number(x, 'f', xStep < 1.0 ? 2 : 1);
    painter.drawText(pos.x() - 20, pos.y() + 20, 40, 20, Qt::AlignCenter,
                     label);
  }

  // Подписи значений на оси Y
  double startY = floor(m_yMin / yStep) * yStep;
  for (double y = startY; y <= m_yMax; y += yStep) {
    if (fabs(y) < 1e-10)
      y = 0;

    QPoint pos = graphToScreen(yAxisX, y);
    QString label = QString::number(y, 'f', yStep < 1.0 ? 2 : 1);
    painter.drawText(pos.x() - 40, pos.y() - 10, 35, 20,
                     Qt::AlignRight | Qt::AlignCenter, label);
  }
}

void PlotWidget::drawAxisArrows(QPainter &painter, const QPoint &xEnd,
                                const QPoint &yEnd) {
  // Стрелка для оси X (вправо)
  QPolygon xArrow;
  xArrow << xEnd << QPoint(xEnd.x() - 10, xEnd.y() - 5)
         << QPoint(xEnd.x() - 10, xEnd.y() + 5);
  painter.drawPolygon(xArrow);

  // Стрелка для оси Y (вверх)
  QPolygon yArrow;
  yArrow << yEnd << QPoint(yEnd.x() - 5, yEnd.y() + 10)
         << QPoint(yEnd.x() + 5, yEnd.y() + 10);
  painter.drawPolygon(yArrow);

  // Заливка стрелок
  painter.setBrush(Qt::black);
  painter.drawPolygon(xArrow);
  painter.drawPolygon(yArrow);
}

void PlotWidget::drawLabels(QPainter &painter) {
  painter.setFont(m_labelFont);
  painter.setPen(Qt::black);

  // Заголовок (вверху по центру)
  painter.drawText(QRect(0, 10, width(), 30), Qt::AlignCenter, m_title);

  // Название оси X (внизу по центру)
  painter.drawText(QRect(0, height() - 25, width(), 25), Qt::AlignCenter,
                   m_xLabel);

  // Название оси Y (слева вертикально)
  painter.save();
  painter.translate(15, height() / 2.0);
  painter.rotate(-90);
  painter.drawText(QRect(0, 0, height(), 25), Qt::AlignCenter, m_yLabel);
  painter.restore();
}

void PlotWidget::drawAxes(QPainter &painter) {
  painter.setPen(m_axisPen);

  // Определяем позиции осей с приклеиванием к граням
  double yAxisX = (m_xMin >= 0) ? m_xMin : (m_xMax < 0) ? m_xMax : 0;
  double xAxisY = (m_yMin >= 0) ? m_yMin : (m_yMax < 0) ? m_yMax : 0;

  //  Расчет шага один раз для сетки и подписей
  double xStep = calculateStep(m_xMax - m_xMin);
  double yStep = calculateStep(m_yMax - m_yMin);

  // Ось X
  QPoint xStart = graphToScreen(m_xMin, xAxisY);
  QPoint xEnd = graphToScreen(m_xMax, xAxisY);
  painter.drawLine(xStart, xEnd);

  // Ось X
  QPoint yStart = graphToScreen(yAxisX, m_yMin);
  QPoint yEnd = graphToScreen(yAxisX, m_yMax);
  painter.drawLine(yStart, yEnd);

  // Отрисовка вспомогательных элементов (сетки, стрелок на концах осей,
  // масштаба)
  drawAxisArrows(painter, xEnd, yEnd);
  drawGrid(painter, xStep, yStep);
  drawAxisLabels(painter, xAxisY, yAxisX, xStep, yStep);
  drawLabels(painter); // Отрисовка заголовка и названий осей
}

void PlotWidget::drawLayers(QPainter &painter) {
  foreach (PlotLayer *layer, m_layers) {
    if (!layer->visible || !layer->attachedFeature)
      continue;

    // Всю отрисовку делает фича.
    layer->attachedFeature->drawOnLayer(painter, layer, *this);
  }
}

QPoint PlotWidget::graphToScreen(double x, double y) const {
  // Преобразование координат графика в экранные координаты
  int screenX = m_offsetX + x * m_scaleX;
  int screenY =
      m_offsetY - y * m_scaleY; // Ось Y инвертированна (экранные координаты)

  return QPoint(screenX, screenY);
}

void PlotWidget::updateCoordinateTransform() {
  // Область для рисования графика (с отступами для осей и подписей)
  m_plotArea = rect().adjusted(60, 40, -40, -60);

  // Масштаб по осям
  m_scaleX = m_plotArea.width() / (m_xMax - m_xMin);
  m_scaleY = m_plotArea.height() / (m_yMax - m_yMin);

  // Смещение (левый нижний угол области графика)
  m_offsetX = m_plotArea.left() - m_xMin * m_scaleX;
  m_offsetY = m_plotArea.bottom() +
              m_yMin * m_scaleY; // + потому что ось Y инвертирована

  qDebug() << "Transform: scaleX" << m_scaleX << "scaleY" << m_scaleY
           << "offsetX" << m_offsetX << "offsetY" << m_offsetY;
}
