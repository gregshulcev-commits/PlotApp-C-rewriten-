// DataManager.h
#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QColor>
#include <QDateTime>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QString>
#include <QVector>

/**
 * @class DataManager
 * @brief Центральный менеджер данных для управления графиками и слоями
 *
 * DataManager является ядром приложения, отвечающим за хранение, управление
 * и координацию всех данных графиков. Обеспечивает thread-safe операции
 * и уведомляет подписчиков об изменениях через систему сигналов Qt.
 */
class DataManager : public QObject {
  Q_OBJECT

public:
  /**
   * @struct DataSeries
   * @brief Структура для хранения данных одного графика
   *
   * Содержит координаты точек, метаинформацию и свойства отображения.
   * Каждый график принадлежит определенному слою и может быть скрыт или
   * показан.
   */
  struct DataSeries {
    QString id;            ///< Уникальный идентификатор графика
    QString name;          ///< Человеко-читаемое название
    QString layerName;     ///< Имя слоя, к которому принадлежит график
    QVector<double> xData; ///< Данные по оси X
    QVector<double> yData; ///< Данные по оси Y
    QColor color;          ///< Цвет графика
    bool visible;          ///< Флаг видимости графика
    int pointCount() const { return xData.size(); } // Кол-во точек
    QDateTime creationTime;                         // Время создания
    QString description;                            // Описание графика

    /**
     * @brief Конструктор DataSeries с параметрами по умолчанию
     * @param seriesName Название графика
     * @param parentLayer Имя родительского слоя
     */
    DataSeries(const QString &seriesName = "Новый график",
               const QString &parentLayer = "Основной слой")
        : name(seriesName), layerName(parentLayer), color(Qt::blue),
          visible(true), creationTime(QDateTime::currentDateTime()),
          description("") {
      // Генерация уникального ID на основе времени и имени
      id = QString("%1_%2")
               .arg(seriesName)
               .arg(creationTime.toMSecsSinceEpoch());
    }
  };

  /**
   * @struct PlotLayer
   * @brief Структура для группировки графиков в слои
   *
   * Слои позволяют управлять видимостью и свойствами группы графиков
   * как единым целым. Каждый слой может быть независимо скрыт или показан.
   */
  struct PlotLayer {
    QString name;               ///< Уникальное имя слоя
    QList<DataSeries *> series; ///< Список графиков в слое
    bool visible;               ///< Флаг видимости всего слоя
    QDateTime creationTime;     // Время создания слоя
    QString description;        // Описание слоя
    bool showInLegend;          // Показывать в легенде

    /**
     * @brief Конструктор PlotLayer
     * @param layerName Имя создаваемого слоя
     */
    PlotLayer(const QString &layerName)
        : name(layerName), visible(true),
          creationTime(QDateTime::currentDateTime()), description(""),
          showInLegend(true) {}

    /**
     * @brief Деструктор - освобождает память графиков
     */
    ~PlotLayer() { qDeleteAll(series); }
  };

  /**
   * @brief Конструктор DataManager
   * @param parent Родительский QObject (обычно nullptr для синглтонов)
   */
  explicit DataManager(QObject *parent = nullptr);

  /**
   * @brief Деструктор - освобождает ресурсы
   */
  ~DataManager();

  // === УПРАВЛЕНИЕ ДАННЫМИ ГРАФИКОВ ===

  /**
   * @brief Добавляет новый график в указанный слой
   * @param layerName Имя слоя для добавления (будет создан если не существует)
   * @param xData Вектор данных по оси X
   * @param yData Вектор данных по оси Y
   * @param seriesName Название графика (опционально)
   * @param seriesColor Цвет графика (опционально)
   * @return QString Уникальный ID созданного графика или пустая строка при
   * ошибке
   *
   * @note Если векторы xData и yData разной длины, используется минимальная
   * длина
   */
  QString addDataSeries(const QString &layerName, const QVector<double> &xData,
                        const QVector<double> &yData,
                        const QString &seriesName = "Новый график",
                        const QColor &seriesColor = Qt::blue);

  /**
   * @brief Удаляет график по его ID
   * @param seriesId Уникальный идентификатор графика для удаления
   * @return bool true если график был найден и удален, иначе false
   */
  bool removeDataSeries(const QString &seriesId);

  /**
   * @brief Возвращает график по его ID
   * @param seriesId Уникальный идентификатор графика
   * @return DataSeries* Указатель на график или nullptr если не найден
   *
   * @warning Возвращаемый указатель действителен только до следующей операции с
   * DataManager
   * @note Для thread-safe доступа используйте мьютекс при работе с указателем
   */
  DataSeries *getSeries(const QString &seriesId);

  /**
   * @brief Возвращает список всех графиков
   * @return QList<DataSeries*> Список указателей на все графики
   *
   * @warning Для thread-safe операций заблокируйте мьютекс перед использованием
   */
  QList<DataSeries *> getAllSeries() const;

  /**
   * @brief Ищет график по имени
   * @param name Имя графика для поиска
   * @return DataSeries* Указатель на первый найденный график или nullptr
   */
  DataSeries *findSeriesByName(const QString &name) const;

  // === УПРАВЛЕНИЕ СЛОЯМИ ===

  /**
   * @brief Создает новый слой
   * @param layerName Уникальное имя слоя
   * @return bool true если слой создан, false если слой с таким именем уже
   * существует
   */
  bool createLayer(const QString &layerName);

  /**
   * @brief Удаляет слой и все его графики
   * @param layerName Имя слоя для удаления
   * @return bool true если слой найден и удален, иначе false
   */
  bool removeLayer(const QString &layerName);

  /**
   * @brief Устанавливает видимость слоя
   * @param layerName Имя слоя
   * @param visible Флаг видимости (true - показать, false - скрыть)
   * @return bool true если слой найден и обновлен, иначе false
   */
  bool setLayerVisible(const QString &layerName, bool visible);

  /**
   * @brief Возвращает список всех слоев
   * @return QList<PlotLayer*> Список указателей на все слои
   */
  QList<PlotLayer *> getAllLayers() const;

  /**
   * @brief Возвращает слой по имени
   * @param layerName Имя слоя
   * @return PlotLayer* Указатель на слой или nullptr если не найден
   */
  PlotLayer *getLayer(const QString &layerName) const;

  /**
   * @brief Проверяет существование слоя
   * @param layerName Имя слоя для проверки
   * @return bool true если слой существует, иначе false
   */
  bool layerExists(const QString &layerName) const;

  /**
   * @brief Обновляет настройки слоя
   * @param layerName Имя слоя
   * @param newName Новое имя слоя (если пустое - не изменяется)
   * @param description Новое описание
   * @param showInLegend Показывать в легенде
   * @return bool true если успешно
   */
  bool updateLayerSettings(const QString &layerName,
                           const QString &newName = QString(),
                           const QString &description = QString(),
                           bool showInLegend = true);

  /**
   * @brief Переименовывает слой
   * @param oldName Старое имя слоя
   * @param newName Новое имя слоя
   * @return bool true если успешно
   */
  bool renameLayer(const QString &oldName, const QString &newName);

  /**
   * @brief Устанавливает описание слоя
   * @param layerName Имя слоя
   * @param description Описание
   * @return bool true если успешно
   */
  bool setLayerDescription(const QString &layerName,
                           const QString &description);

  /**
   * @brief Устанавливает видимость слоя в легенде
   * @param layerName Имя слоя
   * @param showInLegend Показывать в легенде
   * @return bool true если успешно
   */
  bool setLayerLegendVisibility(const QString &layerName, bool showInLegend);
  /**
   * @brief Обновляет настройки графика
   * @param seriesId ID графика
   * @param newName Новое имя
   * @param newColor Новый цвет
   * @param description Описание
   * @return bool true если успешно
   */
  bool updateSeriesSettings(const QString &seriesId,
                            const QString &newName = QString(),
                            const QColor &newColor = QColor(),
                            const QString &description = QString());

  /**
   * @brief Переименовывает график
   * @param seriesId ID графика
   * @param newName Новое имя
   * @return bool true если успешно
   */
  bool renameSeries(const QString &seriesId, const QString &newName);

  /**
   * @brief Устанавливает цвет графика
   * @param seriesId ID графика
   * @param color Новый цвет
   * @return bool true если успешно
   */
  bool setSeriesColor(const QString &seriesId, const QColor &color);

  /**
   * @brief Возвращает количество точек в графике
   * @param seriesId ID графика
   * @return int Количество точек или -1 если не найден
   */
  int getSeriesPointCount(const QString &seriesId) const;

  /**
   * @brief Возвращает информацию о слое в виде строки (для UI)
   * @param layerName Имя слоя
   * @return QString Информация о слое
   */
  QString getLayerInfo(const QString &layerName) const;

  // === СЛУЖЕБНЫЕ МЕТОДЫ ===

  /**
   * @brief Очищает все данные (удаляет все слои и графики)
   */
  void clearAllData();

  /**
   * @brief Возвращает количество графиков во всех слоях
   * @return int Общее количество графиков
   */
  int getTotalSeriesCount() const;

  /**
   * @brief Возвращает количество слоев
   * @return int Количество слоев
   */
  int getLayersCount() const;

signals:
  /**
   * @brief Сигнал об изменении данных слоя(используется для обновления цвета,
   * названия слоя)
   */
  void layerSettingsChanged(const QString &layerName);

  /**
   * @brief Сигнал об изменении данных графика(используется для обновления
   * цвета, названия слоя)
   */
  void seriesSettingsChanged(const QString &seriesId);

  /**
   * @brief Сигнал об изменении данных (используется для обновления отображения)
   */
  void dataChanged();

  /**
   * @brief Сигнал о добавлении нового графика
   * @param seriesId ID добавленного графика
   */
  void seriesAdded(const QString &seriesId);

  /**
   * @brief Сигнал об удалении графика
   * @param seriesId ID удаленного графика
   */
  void seriesRemoved(const QString &seriesId);

  /**
   * @brief Сигнал об изменении слоев
   */
  void layersChanged();

private:
  QMap<QString, PlotLayer *> m_layers; ///< Контейнер слоев (имя -> слой)
  mutable QMutex m_mutex;              ///< Мьютекс для thread-safe операций

  /**
   * @brief Генерирует уникальный ID для графика
   * @param baseName Базовое имя для ID
   * @return QString Уникальный идентификатор
   */
  QString generateSeriesId(const QString &baseName) const;

  /**
   * @brief Внутренний метод для поиска слоя по имени (без блокировки мьютекса)
   * @param layerName Имя слоя
   * @return PlotLayer* Указатель на слой или nullptr
   */
  PlotLayer *findLayerUnsafe(const QString &layerName) const;
};

#endif // DATAMANAGER_H
