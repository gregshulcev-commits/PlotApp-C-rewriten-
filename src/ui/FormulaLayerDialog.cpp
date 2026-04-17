#include "FormulaLayerDialog.h"

#include "DialogUtil.h"

#include "plotapp/FormulaEvaluator.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace plotapp::ui {

FormulaLayerDialog::FormulaLayerDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Add formula layer");
    applyDialogWindowSize(this, QSize(860, 520), QSize(700, 420));
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    expressionEdit_ = new QLineEdit("sin(x)", this);
    nameEdit_ = new QLineEdit(expressionEdit_->text(), this);
    xMinSpin_ = new QDoubleSpinBox(this);
    xMaxSpin_ = new QDoubleSpinBox(this);
    samplesSpin_ = new QSpinBox(this);
    for (auto* spin : {xMinSpin_, xMaxSpin_}) {
        spin->setRange(-1e9, 1e9);
        spin->setDecimals(6);
    }
    xMinSpin_->setValue(-10.0);
    xMaxSpin_->setValue(10.0);
    samplesSpin_->setRange(2, 100000);
    samplesSpin_->setValue(512);
    form->addRow("Layer name", nameEdit_);
    form->addRow("Formula", expressionEdit_);
    form->addRow("X min", xMinSpin_);
    form->addRow("X max", xMaxSpin_);
    form->addRow("Samples", samplesSpin_);
    layout->addLayout(form);

    autoSuggestedName_ = expressionEdit_->text();
    connect(expressionEdit_, &QLineEdit::textChanged, this, &FormulaLayerDialog::updateSuggestedLayerName);
    connect(nameEdit_, &QLineEdit::textEdited, this, &FormulaLayerDialog::onLayerNameEdited);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &FormulaLayerDialog::validateAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString FormulaLayerDialog::layerName() const { return nameEdit_->text(); }
QString FormulaLayerDialog::expression() const { return expressionEdit_->text(); }
double FormulaLayerDialog::xMin() const { return xMinSpin_->value(); }
double FormulaLayerDialog::xMax() const { return xMaxSpin_->value(); }
int FormulaLayerDialog::samples() const { return samplesSpin_->value(); }

void FormulaLayerDialog::updateSuggestedLayerName() {
    const QString suggested = expressionEdit_->text().trimmed().isEmpty() ? QString("Formula") : expressionEdit_->text().trimmed();
    const bool shouldApply = autoNameMode_ || nameEdit_->text().trimmed().isEmpty() || nameEdit_->text() == autoSuggestedName_;
    autoSuggestedName_ = suggested;
    if (!shouldApply) return;

    QSignalBlocker blocker(nameEdit_);
    nameEdit_->setText(suggested);
    autoNameMode_ = true;
}

void FormulaLayerDialog::onLayerNameEdited(const QString& text) {
    autoNameMode_ = text.trimmed().isEmpty() || text == autoSuggestedName_;
}

void FormulaLayerDialog::setSuggestedRange(double xMin, double xMax) {
    if (!std::isfinite(xMin) || !std::isfinite(xMax)) return;
    if (xMin > xMax) std::swap(xMin, xMax);
    if (xMin == xMax) {
        xMin -= 1.0;
        xMax += 1.0;
    }
    xMinSpin_->setValue(xMin);
    xMaxSpin_->setValue(xMax);
}

void FormulaLayerDialog::validateAndAccept() {
    try {
        double minValue = xMin();
        double maxValue = xMax();
        int sampleCount = samples();
        std::string rawExpression = expression().toStdString();
        FormulaEvaluator::validate(rawExpression);
        if (!std::isfinite(minValue) || !std::isfinite(maxValue)) {
            throw std::runtime_error("Formula layer range must be finite");
        }
        if (minValue == maxValue) {
            throw std::runtime_error("Formula layer range cannot be zero-width");
        }
        if (minValue > maxValue) std::swap(minValue, maxValue);
        sampleCount = std::max(sampleCount, 2);
        (void)FormulaEvaluator::sample(rawExpression, minValue, maxValue, std::min(sampleCount, 32));
        accept();
    } catch (const std::exception& ex) {
        QMessageBox::warning(this, "Formula error", QString::fromUtf8(ex.what()));
        expressionEdit_->setFocus();
        expressionEdit_->selectAll();
    }
}

} // namespace plotapp::ui
