#ifndef DATAMANAGER_H
#define DATAMANAGER_H

// Базовые Qt библиотеки
#include <QColor>    // Для хранения цвета графиков
#include <QDateTime> // Для времени создания слоев
#include <QDebug>    // Для отладочного вывода
#include <QMap>      // Для хранения слоев (ключ-значение)
#include <QObject>   // Для сигналов/слотов и родительской системы Qt
#include <QString>   // Для работы со строками
#include <QVector>   // Для хранения массивов данных (координаты X,Y)

// Предварительное объявление классов (чтобы избежать циклических включений)
class Feature;

/**
 * @struct DataSeries
 * @brief Структура для хранения данных одного графика
 *
 * Содержит массивы координат и метаинформацию о графике.
 * Каждый график в системе представлен этой структурой.
 */
struct DataSeries {
  QVector<double> xData; ///< Массив координат X (абсциссы)
  QVector<double> yData; ///< Массив координат Y (ординаты)
  QColor color;          ///< Цвет отображения графика
  QString name;          ///< Название графика (для легенды)

  /**
   * @brief Конструктор с параметрами по умолчанию
   * @param seriesName Название графика
   * @param seriesColor Цвет графика
   */
  DataSeries(const QString &seriesName = "Новый график",
             const QColor &seriesColor = Qt::blue)
      : color(seriesColor), name(seriesName) {}
};

/**
 * @class PlotLayer
 * @brief Класс представляющий один слой для отображения графика
 *
 * Слой - это независимый визуальный элемент, который:
 * - Содержит данные графика (DataSeries)
 * - Имеет настройки отображения (видимость, легенда)
 * - Может быть привязан к конкретной фиче (Feature)
 * - Автоматически уведомляет систему об изменениях
 */
class PlotLayer : public QObject {
Q_OBJECT // Макрос Qt для поддержки сигналов/слотов

    public :
    // === ДАННЫЕ СЛОЯ ===
    QString name;           ///< Уникальное имя слоя (идентификатор)
  QString type;             ///< Тип слоя: "graph", "errors", "approximation"
  DataSeries data;          ///< Данные графика (координаты точек)
  bool visible;             ///< Флаг видимости слоя (true - показан)
  bool showInLegend;        ///< Показывать ли слой в легенде
  QDateTime creationTime;   ///< Время создания слоя
  QString description;      ///< Текстовое описание слоя
  Feature *attachedFeature; ///< Указатель на фичу, управляющую слоем

  /**
   * @brief Конструктор слоя
   * @param layerName Уникальное имя слоя
   * @param layerType Тип слоя ("graph", "errors", etc.)
   * @param feature Указатель на фичу (может быть nullptr)
   */
  PlotLayer(const QString &layerName, const QString &layerType = "graph",
            Feature *feature = nullptr);

  /**
   * @brief Устанавливает новые данные для графика
   * @param newData Новые данные графика
   *
   * При изменении данных испускает сигнал dataChanged()
   */
  void setData(const DataSeries &newData);

signals:
  /**
   * @brief Сигнал об изменении данных в слое
   *
   * Испускается когда данные графика обновляются.
   * PlotWidget подписывается на этот сигнал для перерисовки.
   */
  void dataChanged();
};

/**
 * @class DataManager
 * @brief Центральный менеджер данных, слоев и фич приложения
 *
 * DataManager является ядром системы и отвечает за:
 * - Создание, удаление и управление слоями (PlotLayer)
 * - Регистрацию и поиск фич (Feature)
 * - Сериализацию и десериализацию проектов
 * - Координацию взаимодействия между компонентами
 * - Уведомление системы об изменениях через сигналы
 */
class DataManager : public QObject {
  Q_OBJECT

public:
  explicit DataManager(QObject *parent = nullptr);
  ~DataManager();

  // === УПРАВЛЕНИЕ СЛОЯМИ ===

  /**
   * @brief Создает новый слой для отображения графика
   * @param layerName Уникальное имя слоя
   * @param layerType Тип слоя ("graph", "errors", etc.)
   * @return PlotLayer* Указатель на созданный слой или nullptr при ошибке
   *
   * @throws Генерирует предупреждение если имя пустое или не уникальное
   * @emits layerCreated() после успешного создания
   */
  PlotLayer *createLayer(const QString &layerName,
                         const QString &layerType = "graph");

  /**
   * @brief Удаляет слой и все связанные с ним ресурсы
   * @param layerName Имя слоя для удаления
   * @return bool true если слой найден и удален, иначе false
   *
   * @emits layerRemoved() после успешного удаления
   * @emits dataChanged() для обновления интерфейса
   */
  bool removeLayer(const QString &layerName);

  /**
   * @brief Возвращает слой по имени
   * @param layerName Имя искомого слоя
   * @return PlotLayer* Указатель на слой или nullptr если не найден
   */
  PlotLayer *getLayer(const QString &layerName) const;

  /**
   * @brief Устанавливает видимость слоя
   * @param layerName Имя слоя
   * @param visible true - показать слой, false - скрыть
   * @return bool true если слой найден и обновлен, иначе false
   *
   * @emits layerChanged() если видимость изменилась
   */
  bool setLayerVisible(const QString &layerName, bool visible);

  /**
   * @brief Переименовывает слой
   * @param oldName Текущее имя слоя
   * @param newName Новое имя слоя
   * @return bool true если переименование успешно, иначе false
   *
   * @throws Генерирует предупреждение если oldName не существует или newName
   * уже занят
   * @emits layerChanged() после успешного переименования
   */
  bool renameLayer(const QString &oldName, const QString &newName);

  /**
   * @brief Установить описание слоя
   * @param layerName Имя слоя, которому нужно добавить описание
   * @param description Описание слоя
   * @return bool true если добавление успешно, иначе false
   *
   * @throws Генерирует предупреждение, если слой не найден
   * @emits layerChanged() после успешного добавления описания
   */
  bool setLayerDescription(const QString &layerName,
                           const QString &description);

  /**
   * @brief Возвращает список всех слоев в системе
   * @return QList<PlotLayer*> Список указателей на все слои
   */
  QList<PlotLayer *> getAllLayers() const;

  // === РЕЕСТР ФИЧ ===

  /**
   * @brief Регистрирует фичу в системе
   * @param featureName Уникальное имя фичи
   * @param feature Указатель на объект фичи
   *
   * @throws Генерирует предупреждение если имя уже занято
   */
  bool registerFeature(const QString &featureName, Feature *feature);

  /**
   * @brief Возвращает фичу по имени
   * @param featureName Имя фичи
   * @return Feature* Указатель на фичу или nullptr если не найдена
   */
  Feature *getFeature(const QString &featureName) const;

signals:
  /**
   * @brief Сигнал о создании нового слоя
   * @param layer Указатель на созданный слой
   */
  void layerCreated(PlotLayer *layer);

  /**
   * @brief Сигнал об удалении слоя
   * @param layerName Имя удаленного слоя
   */
  void layerRemoved(const QString &layerName);

  /**
   * @brief Сигнал об изменении настроек слоя
   * @param layerName Имя измененного слоя
   *
   * Испускается при изменении видимости, переименовании и т.д.
   */
  void layerChanged(const QString &layerName);

  /**
   * @brief Сигнал об изменении данных в системе
   *
   * Испускается при любых изменениях затрагивающих отображение:
   * - Создание/удаление слоев
   * - Изменение видимости
   * - Обновление данных графиков
   */
  void dataChanged();

private:
  QMap<QString, PlotLayer *> m_layers;        ///< Хранилище слоев [имя → слой]
  QMap<QString, Feature *> m_featureRegistry; ///< Реестр фич [имя → фича]
};

#endif // DATAMANAGER_H
