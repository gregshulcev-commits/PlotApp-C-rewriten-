#ifndef FEATURE_H
#define FEATURE_H

#include <QObject>
#include <QPainter>

// Предварительные объявления
class PlotLayer;
class PlotWidget;

/**
 * @class Feature
 * @brief Базовый класс для всех функциональных модулей системы
 *
 * Feature представляет собой расширяемый модуль, который:
 * - Реализует специфическую функциональность (графики, погрешности,
 * аппроксимация)
 * - Самостоятельно отрисовывает свои данные на слое
 * - Может обрабатывать пользовательский ввод
 * - Регистрируется в DataManager для автоматического обнаружения
 */
class Feature : public QObject {
  Q_OBJECT

public:
  explicit Feature(QObject *parent = nullptr) : QObject(parent) {}
  virtual ~Feature() = default;

  /**
   * @brief Основной метод отрисовки фичи на слое
   * @param painter QPainter для отрисовки
   * @param layer Слой, на котором происходит отрисовка
   * @param canvas Холст (PlotWidget) с координатной системой
   *
   * Каждая фича реализует этот метод по-своему в зависимости от своей
   * функциональности.
   */
  virtual void drawOnLayer(QPainter &painter, PlotLayer *layer,
                           PlotWidget &canvas) = 0;

  /**
   * @brief Возвращает имя фичи для регистрации в DataManager
   * @return Уникальное имя фичи
   */
  virtual QString getName() const = 0;

signals:
  /**
   * @brief Сигнал о необходимости обновления отображения
   */
  void updateRequested();

  /**
   * @brief Сигнал об ошибке в работе фичи
   * @param errorMessage Описание ошибки
   */
  void errorOccurred(const QString &errorMessage);
};

#endif // FEATURE_H
