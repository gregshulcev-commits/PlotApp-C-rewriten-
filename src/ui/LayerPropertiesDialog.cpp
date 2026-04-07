#include "LayerPropertiesDialog.h"

#include "plotapp/FormulaEvaluator.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace plotapp::ui {
namespace {

QWidget* makeColorRow(QWidget* parent, QLineEdit** editOut, QPushButton** buttonOut, const QString& value) {
    auto* edit = new QLineEdit(value, parent);
    auto* button = new QPushButton("Choose...", parent);
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(edit);
    layout->addWidget(button);
    *editOut = edit;
    *buttonOut = button;
    return row;
}

} // namespace

LayerPropertiesDialog::LayerPropertiesDialog(plotapp::Layer layer, QWidget* parent)
    : QDialog(parent), layer_(std::move(layer)) {
    setWindowTitle("Layer properties");
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    nameEdit_ = new QLineEdit(QString::fromStdString(layer_.name), this);
    auto* colorRow = makeColorRow(this, &colorEdit_, &colorButton_, QString::fromStdString(layer_.style.color));
    auto* secondaryColorRow = makeColorRow(this, &secondaryColorEdit_, &secondaryColorButton_, QString::fromStdString(layer_.style.secondaryColor));
    connect(colorButton_, &QPushButton::clicked, this, &LayerPropertiesDialog::chooseColor);
    connect(secondaryColorButton_, &QPushButton::clicked, this, &LayerPropertiesDialog::chooseSecondaryColor);

    lineWidthSpin_ = new QSpinBox(this);
    lineWidthSpin_->setRange(1, 12);
    lineWidthSpin_->setValue(layer_.style.lineWidth);

    visibleCheck_ = new QCheckBox(this);
    visibleCheck_->setChecked(layer_.visible);
    markersCheck_ = new QCheckBox(this);
    markersCheck_->setChecked(layer_.style.showMarkers);
    connectPointsCheck_ = new QCheckBox(this);
    connectPointsCheck_->setChecked(layer_.style.connectPoints);

    form->addRow("Name", nameEdit_);
    form->addRow("Primary color", colorRow);
    form->addRow("Secondary color", secondaryColorRow);
    form->addRow("Line width", lineWidthSpin_);
    form->addRow("Visible", visibleCheck_);
    form->addRow("Show markers", markersCheck_);
    form->addRow("Connect points", connectPointsCheck_);
    layout->addLayout(form);

    auto* legendBox = new QGroupBox("Legend", this);
    auto* legendForm = new QFormLayout(legendBox);
    legendVisibleCheck_ = new QCheckBox(this);
    legendVisibleCheck_->setChecked(layer_.legendVisible);
    legendTextEdit_ = new QLineEdit(QString::fromStdString(layer_.legendText.empty() ? layer_.name : layer_.legendText), this);
    legendXSpin_ = new QDoubleSpinBox(this);
    legendYSpin_ = new QDoubleSpinBox(this);
    for (auto* spin : {legendXSpin_, legendYSpin_}) {
        spin->setRange(0.0, 1.0);
        spin->setDecimals(3);
        spin->setSingleStep(0.01);
    }
    legendXSpin_->setValue(layer_.legendAnchorX);
    legendYSpin_->setValue(layer_.legendAnchorY);
    legendForm->addRow("Visible", legendVisibleCheck_);
    legendForm->addRow("Text", legendTextEdit_);
    legendForm->addRow("Anchor X (0..1)", legendXSpin_);
    legendForm->addRow("Anchor Y (0..1)", legendYSpin_);
    layout->addWidget(legendBox);

    auto* formulaBox = new QGroupBox("Formula (used when layer is formula-based)", this);
    auto* formulaForm = new QFormLayout(formulaBox);
    formulaEdit_ = new QLineEdit(QString::fromStdString(layer_.formulaExpression), this);
    formulaXMinSpin_ = new QDoubleSpinBox(this);
    formulaXMaxSpin_ = new QDoubleSpinBox(this);
    formulaSamplesSpin_ = new QSpinBox(this);
    for (auto* spin : {formulaXMinSpin_, formulaXMaxSpin_}) {
        spin->setRange(-1e9, 1e9);
        spin->setDecimals(6);
    }
    formulaSamplesSpin_->setRange(2, 100000);
    formulaXMinSpin_->setValue(layer_.formulaXMin);
    formulaXMaxSpin_->setValue(layer_.formulaXMax);
    formulaSamplesSpin_->setValue(layer_.formulaSamples);
    formulaForm->addRow("Expression", formulaEdit_);
    formulaForm->addRow("X min", formulaXMinSpin_);
    formulaForm->addRow("X max", formulaXMaxSpin_);
    formulaForm->addRow("Samples", formulaSamplesSpin_);
    layout->addWidget(formulaBox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* deleteButton = buttons->addButton("Delete layer", QDialogButtonBox::DestructiveRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &LayerPropertiesDialog::validateAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(deleteButton, &QPushButton::clicked, this, &LayerPropertiesDialog::requestDelete);
    layout->addWidget(buttons);
    updateColorPreview();
}

void LayerPropertiesDialog::chooseColor() {
    const QColor selected = QColorDialog::getColor(QColor(QString::fromStdString(layer_.style.color)), this, "Choose layer color");
    if (!selected.isValid()) return;
    colorEdit_->setText(selected.name());
    updateColorPreview();
}

void LayerPropertiesDialog::chooseSecondaryColor() {
    const QColor fallback(QString::fromStdString(layer_.style.secondaryColor));
    const QColor selected = QColorDialog::getColor(fallback.isValid() ? fallback : QColor(QString::fromStdString(layer_.style.color)), this, "Choose secondary layer color");
    if (!selected.isValid()) return;
    secondaryColorEdit_->setText(selected.name());
    updateColorPreview();
}

void LayerPropertiesDialog::requestDelete() {
    deleteRequested_ = true;
    accept();
}

void LayerPropertiesDialog::validateAndAccept() {
    if (layer_.type != plotapp::LayerType::FormulaSeries) {
        accept();
        return;
    }

    try {
        double minValue = formulaXMinSpin_->value();
        double maxValue = formulaXMaxSpin_->value();
        int sampleCount = std::max(2, formulaSamplesSpin_->value());
        std::string expression = formulaEdit_->text().toStdString();
        FormulaEvaluator::validate(expression);
        if (!std::isfinite(minValue) || !std::isfinite(maxValue)) {
            throw std::runtime_error("Formula layer range must be finite");
        }
        if (minValue == maxValue) {
            throw std::runtime_error("Formula layer range cannot be zero-width");
        }
        if (minValue > maxValue) std::swap(minValue, maxValue);
        (void)FormulaEvaluator::sample(expression, minValue, maxValue, std::min(sampleCount, 32));
        accept();
    } catch (const std::exception& ex) {
        QMessageBox::warning(this, "Formula error", QString::fromUtf8(ex.what()));
        formulaEdit_->setFocus();
        formulaEdit_->selectAll();
    }
}

void LayerPropertiesDialog::updateColorPreview() {
    colorButton_->setStyleSheet(QString("background:%1;").arg(colorEdit_->text()));
    secondaryColorButton_->setStyleSheet(QString("background:%1;").arg(secondaryColorEdit_->text()));
}

plotapp::Layer LayerPropertiesDialog::result() const {
    auto copy = layer_;
    copy.name = nameEdit_->text().toStdString();
    copy.style.color = colorEdit_->text().toStdString();
    copy.style.secondaryColor = secondaryColorEdit_->text().toStdString();
    copy.style.lineWidth = lineWidthSpin_->value();
    copy.visible = visibleCheck_->isChecked();
    copy.style.showMarkers = markersCheck_->isChecked();
    copy.style.connectPoints = connectPointsCheck_->isChecked();
    copy.legendVisible = legendVisibleCheck_->isChecked();
    copy.legendText = legendTextEdit_->text().toStdString();
    copy.legendAnchorX = legendXSpin_->value();
    copy.legendAnchorY = legendYSpin_->value();
    copy.formulaExpression = formulaEdit_->text().toStdString();
    copy.formulaXMin = formulaXMinSpin_->value();
    copy.formulaXMax = formulaXMaxSpin_->value();
    copy.formulaSamples = formulaSamplesSpin_->value();
    return copy;
}

bool LayerPropertiesDialog::deleteRequested() const {
    return deleteRequested_;
}

} // namespace plotapp::ui
