#include "SettingsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

namespace plotapp::ui {

SettingsDialog::SettingsDialog(const QString& theme, int scalePercent, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Settings");
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    themeBox_ = new QComboBox(this);
    themeBox_->addItem("Dark", "dark");
    themeBox_->addItem("Light", "light");
    const int themeIndex = std::max(0, themeBox_->findData(theme));
    themeBox_->setCurrentIndex(themeIndex);
    scaleBox_ = new QSpinBox(this);
    scaleBox_->setRange(80, 200);
    scaleBox_->setSingleStep(5);
    scaleBox_->setValue(scalePercent);
    form->addRow("Theme", themeBox_);
    form->addRow("UI scale (%)", scaleBox_);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString SettingsDialog::theme() const { return themeBox_->currentData().toString(); }
int SettingsDialog::scalePercent() const { return scaleBox_->value(); }

} // namespace plotapp::ui
