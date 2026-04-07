#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

namespace plotapp::ui {

class FormulaLayerDialog : public QDialog {
    Q_OBJECT
public:
    explicit FormulaLayerDialog(QWidget* parent = nullptr);

    QString layerName() const;
    QString expression() const;
    double xMin() const;
    double xMax() const;
    int samples() const;

private slots:
    void validateAndAccept();

private:
    QLineEdit* nameEdit_ {nullptr};
    QLineEdit* expressionEdit_ {nullptr};
    QDoubleSpinBox* xMinSpin_ {nullptr};
    QDoubleSpinBox* xMaxSpin_ {nullptr};
    QSpinBox* samplesSpin_ {nullptr};
};

} // namespace plotapp::ui
