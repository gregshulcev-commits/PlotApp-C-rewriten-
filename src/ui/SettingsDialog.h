#pragma once

#include <QDialog>

class QComboBox;
class QSpinBox;

namespace plotapp::ui {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const QString& theme, int scalePercent, QWidget* parent = nullptr);

    QString theme() const;
    int scalePercent() const;

private:
    QComboBox* themeBox_ {nullptr};
    QSpinBox* scaleBox_ {nullptr};
};

} // namespace plotapp::ui
