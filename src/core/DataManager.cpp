#include "DataManager.h"
#include "Feature.h"

// ==================== PlotLayer ====================

PlotLayer::PlotLayer(const QString &layerName, const QString &layerType,
                     Feature *feature)
    : name(layerName), type(layerType), visible(true), showInLegend(true),
      creationTime(QDateTime::currentDateTime()), description(""),
      attachedFeature(feature) {
  qDebug() << "PlotLayer: создан слой" << layerName << "типа" << layerType;
}

void PlotLayer::setData(const DataSeries &newData) {
  data = newData;
  qDebug() << "PlotLayer: данные обновлены в слое" << name
           << "точек:" << data.xData.size();
  emit dataChanged();
}

// ==================== DataManager ====================

DataManager::DataManager(QObject *parent) : QObject(parent) {
  qDebug() << "DataManager: инициализирован";
}

DataManager::~DataManager() {
  // Очищаем все слои
  qDeleteAll(m_layers);
  m_layers.clear();
  qDebug() << "DataManager: дезинтегрирован";
}

PlotLayer *DataManager::createLayer(const QString &layerName,
                                    const QString &layerType) {
  // Проверка 1: Имя должно быть пустым
  if (layerName.isEmpty()) {
    qWarning() << "DataManager: Попытка создать слой с пустым именем";
    return nullptr;
  }

  // Проверка 2: Имя должно быть уникальным
  if (m_layers.contains(layerName)) {
    qWarning() << "DataManager: слой с именем" << layerName << "уже существует";
    return nullptr;
  }

  // Проверка 3: Тип слоя не должен быть пустым
  if (layerType.isEmpty()) {
    qWarning() << "DataManager: попытка создать слой с пустым типом";
    return nullptr;
  }

  // Создаем новый слой
  PlotLayer *newLayer = new PlotLayer(layerName, layerType);
  m_layers[layerName] = newLayer;

  qDebug() << "DataManager: создан слой" << layerName << "типа" << layerType;

  emit layerCreated(newLayer);
  emit dataChanged();

  return newLayer;
}

bool DataManager::removeLayer(const QString &layerName) {
  // Проверка 1: Слой должен существовать
  if (!m_layers.contains(layerName)) {
    qWarning() << "DataManager: слой" << layerName << "не найден для удаления";
    return false;
  }

  // Достаем слой из хранилища
  PlotLayer *layer = m_layers.take(layerName);

  // Освобождаем память
  delete layer;

  qDebug() << "DataManager: удален слой" << layerName;

  // Уведовить систему об удалении
  emit layerRemoved(layerName);
  emit dataChanged();

  return true;
}

PlotLayer *DataManager::getLayer(const QString &layerName) const {
  return m_layers.value(layerName, nullptr);
}

bool DataManager::setLayerVisible(const QString &layerName, bool visible) {
  PlotLayer *layer = getLayer(layerName);
  if (!layer) {
    qWarning() << "DataManager: слой" << layerName << "не найден";
    return false;
  }
  if (layer->visible != visible) {
    layer->visible = visible;
    qDebug() << "DataManager: видимость слоя" << layerName << "установлена в "
             << visible;
    emit layerChanged(layerName);
    emit dataChanged();
  }
  return true;
}

bool DataManager::renameLayer(const QString &oldName, const QString &newName) {
  // Проверка существования старого имени
  if (!m_layers.contains(oldName)) {
    qWarning() << "DataManager: слой" << oldName << "не найден";
    return false;
  }

  // Проверка уникальности новго имени
  if (m_layers.contains(newName)) {
    qWarning() << "DataManager: слой" << newName << "уже существует";
    return false;
  }

  // Переименование
  PlotLayer *layer = m_layers.take(oldName);
  layer->name = newName;
  m_layers[newName] = layer;
  qDebug() << "DataManager: слой" << oldName << "переменован в" << newName;
  emit layerChanged(newName);
  return true;
}

bool DataManager::setLayerDescription(const QString &layerName,
                                      const QString &description) {
  PlotLayer *layer = getLayer(layerName);
  if (!layer) {
    qWarning() << "DataManager: слой" << layerName << "не найден";
    return false;
  }

  layer->description = description;
  qDebug() << "DataManager: описание слоя" << layerName << "обновлено";
  emit layerChanged(layerName);

  return true;
}

QList<PlotLayer *> DataManager::getAllLayers() const {
  return m_layers.values();
}

bool DataManager::registerFeature(const QString &featureName,
                                  Feature *feature) {
  // Проверка уникальности имени
  if (m_featureRegistry.contains(featureName)) {
    qWarning() << "DataManager: фича" << featureName << "уже зарегистрирована";
    return false;
  }

  // Проверка валидности указателя
  if (!feature) {
    qWarning() << "DataManager: попытка зарегистрировать nullptr фичу";
    return false;
  }

  // Регистрация фичи
  m_featureRegistry[featureName] = feature;
  qDebug() << "DataManager: зарегистрирована фича" << featureName;
  return true;
}

Feature *DataManager::getFeature(const QString &featureName) const {
  return m_featureRegistry.value(featureName, nullptr);
}
