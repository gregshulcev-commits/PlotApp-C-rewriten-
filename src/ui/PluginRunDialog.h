#pragma once

#include "plotapp/PluginManager.h"
#include "plotapp/Project.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QWidget;

namespace plotapp::ui {

class PluginRunDialog : public QDialog {
    Q_OBJECT
public:
    explicit PluginRunDialog(const std::vector<plotapp::PluginInfo>& plugins,
                             const plotapp::Layer* sourceLayer,
                             QWidget* parent = nullptr);

    QString pluginId() const;
    QString params() const;

private slots:
    void updateDescription();

private:
    const plotapp::Layer* sourceLayer_ {nullptr};
    std::vector<plotapp::PluginInfo> plugins_;
    QComboBox* pluginBox_ {nullptr};
    QLabel* descriptionLabel_ {nullptr};
    QLineEdit* paramsEdit_ {nullptr};
    QWidget* degreeRow_ {nullptr};
    QSpinBox* degreeSpin_ {nullptr};
    QWidget* linearFitRow_ {nullptr};
    QCheckBox* linearFitAxisIntersectionsCheck_ {nullptr};
    QWidget* errorColumnRow_ {nullptr};
    QComboBox* errorColumnBox_ {nullptr};
    QLabel* errorColumnHint_ {nullptr};
};

} // namespace plotapp::ui
