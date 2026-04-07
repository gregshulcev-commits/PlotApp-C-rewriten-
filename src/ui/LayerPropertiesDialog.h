#pragma once

#include "plotapp/Project.h"

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace plotapp::ui {

class LayerPropertiesDialog : public QDialog {
    Q_OBJECT
public:
    explicit LayerPropertiesDialog(plotapp::Layer layer, QWidget* parent = nullptr);

    plotapp::Layer result() const;
    bool deleteRequested() const;

private slots:
    void chooseColor();
    void chooseSecondaryColor();
    void requestDelete();
    void validateAndAccept();

private:
    void updateColorPreview();

    plotapp::Layer layer_;
    bool deleteRequested_ {false};
    QLineEdit* nameEdit_ {nullptr};
    QLineEdit* colorEdit_ {nullptr};
    QPushButton* colorButton_ {nullptr};
    QLineEdit* secondaryColorEdit_ {nullptr};
    QPushButton* secondaryColorButton_ {nullptr};
    QSpinBox* lineWidthSpin_ {nullptr};
    QCheckBox* visibleCheck_ {nullptr};
    QCheckBox* markersCheck_ {nullptr};
    QCheckBox* connectPointsCheck_ {nullptr};
    QCheckBox* legendVisibleCheck_ {nullptr};
    QLineEdit* legendTextEdit_ {nullptr};
    QDoubleSpinBox* legendXSpin_ {nullptr};
    QDoubleSpinBox* legendYSpin_ {nullptr};
    QLineEdit* formulaEdit_ {nullptr};
    QDoubleSpinBox* formulaXMinSpin_ {nullptr};
    QDoubleSpinBox* formulaXMaxSpin_ {nullptr};
    QSpinBox* formulaSamplesSpin_ {nullptr};
};

} // namespace plotapp::ui
