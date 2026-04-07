#include "PluginRunDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace plotapp::ui {
namespace {

QString paramValue(const QString& params, const QString& key) {
    const QString pattern = key + '=';
    const int start = params.indexOf(pattern);
    if (start < 0) return {};
    const int valueStart = start + pattern.size();
    const int end = params.indexOf(';', valueStart);
    return params.mid(valueStart, end < 0 ? -1 : end - valueStart);
}

QString upsertParam(QString params, const QString& key, const QString& value) {
    QStringList items;
    const auto parts = params.split(';', Qt::SkipEmptyParts);
    const QString prefix = key + '=';
    bool inserted = false;
    for (QString part : parts) {
        part = part.trimmed();
        if (part.startsWith(prefix)) {
            if (!value.isEmpty()) items << (prefix + value);
            inserted = true;
        } else if (!part.isEmpty()) {
            items << part;
        }
    }
    if (!inserted && !value.isEmpty()) items << (prefix + value);
    return items.join(';');
}

} // namespace

PluginRunDialog::PluginRunDialog(const std::vector<plotapp::PluginInfo>& plugins,
                                 const plotapp::Layer* sourceLayer,
                                 QWidget* parent)
    : QDialog(parent), sourceLayer_(sourceLayer), plugins_(plugins) {
    setWindowTitle("Apply plugin");
    auto* layout = new QVBoxLayout(this);

    pluginBox_ = new QComboBox(this);
    for (const auto& plugin : plugins_) {
        pluginBox_->addItem(QString::fromStdString(plugin.name), QString::fromStdString(plugin.id));
    }

    descriptionLabel_ = new QLabel(this);
    descriptionLabel_->setWordWrap(true);
    paramsEdit_ = new QLineEdit(this);
    paramsEdit_->setPlaceholderText("key=value;key=value");

    layout->addWidget(new QLabel("Plugin", this));
    layout->addWidget(pluginBox_);
    layout->addWidget(descriptionLabel_);
    layout->addWidget(new QLabel("Parameters", this));
    layout->addWidget(paramsEdit_);

    degreeRow_ = new QWidget(this);
    auto* degreeLayout = new QHBoxLayout(degreeRow_);
    degreeLayout->setContentsMargins(0, 0, 0, 0);
    degreeLayout->addWidget(new QLabel("Polynomial degree", degreeRow_));
    degreeSpin_ = new QSpinBox(degreeRow_);
    degreeSpin_->setRange(1, 12);
    degreeSpin_->setValue(2);
    degreeLayout->addWidget(degreeSpin_);
    degreeLayout->addStretch();
    layout->addWidget(degreeRow_);

    errorColumnRow_ = new QWidget(this);
    auto* errorLayout = new QHBoxLayout(errorColumnRow_);
    errorLayout->setContentsMargins(0, 0, 0, 0);
    errorLayout->addWidget(new QLabel("Imported error column", errorColumnRow_));
    errorColumnBox_ = new QComboBox(errorColumnRow_);
    errorColumnBox_->addItem("(use source errors or uniform=...)", -1);
    if (sourceLayer_ != nullptr) {
        for (std::size_t i = 0; i < sourceLayer_->importedHeaders.size(); ++i) {
            errorColumnBox_->addItem(QString::fromStdString(sourceLayer_->importedHeaders[i]), static_cast<int>(i));
        }
    }
    errorLayout->addWidget(errorColumnBox_);
    layout->addWidget(errorColumnRow_);

    errorColumnHint_ = new QLabel(this);
    errorColumnHint_->setWordWrap(true);
    if (sourceLayer_ != nullptr && !sourceLayer_->importedHeaders.empty()) {
        errorColumnHint_->setText("Choose a numeric column from the imported table to build error bars for the selected layer.");
    } else {
        errorColumnHint_->setText("The selected source layer has no imported table metadata. Use uniform=... or existing source errors.");
    }
    layout->addWidget(errorColumnHint_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(pluginBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, &PluginRunDialog::updateDescription);
    layout->addWidget(buttons);
    updateDescription();
}

QString PluginRunDialog::pluginId() const {
    return pluginBox_->currentData().toString();
}

QString PluginRunDialog::params() const {
    QString out = paramsEdit_->text().trimmed();
    const QString id = pluginId();
    if (id == "newton_polynomial") {
        out = upsertParam(out, "degree", QString::number(degreeSpin_->value()));
    }
    if (id == "error_bars") {
        const int columnIndex = errorColumnBox_->currentData().toInt();
        if (columnIndex >= 0) {
            out = upsertParam(out, "column_index", QString::number(columnIndex));
        }
    }
    return out;
}

void PluginRunDialog::updateDescription() {
    const int index = pluginBox_->currentIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= plugins_.size()) {
        descriptionLabel_->setText({});
        degreeRow_->hide();
        errorColumnRow_->hide();
        errorColumnHint_->hide();
        return;
    }

    const auto& plugin = plugins_[static_cast<std::size_t>(index)];
    const QString defaultParams = QString::fromStdString(plugin.defaultParams);
    const QString id = QString::fromStdString(plugin.id);
    descriptionLabel_->setText(QString::fromStdString(plugin.description));
    paramsEdit_->setText(defaultParams);

    const bool showDegree = id == "newton_polynomial";
    degreeRow_->setVisible(showDegree);
    if (showDegree) {
        bool ok = false;
        const int degree = std::clamp(paramValue(defaultParams, "degree").toInt(&ok), 1, 12);
        degreeSpin_->setValue(ok ? degree : 2);
    }

    const bool showErrorColumn = id == "error_bars";
    errorColumnRow_->setVisible(showErrorColumn);
    errorColumnHint_->setVisible(showErrorColumn);
    if (showErrorColumn) {
        const bool hasImportedColumns = sourceLayer_ != nullptr && !sourceLayer_->importedHeaders.empty();
        errorColumnBox_->setEnabled(hasImportedColumns);
        if (hasImportedColumns) {
            bool ok = false;
            const int columnIndex = paramValue(defaultParams, "column_index").toInt(&ok);
            if (ok) {
                const int comboIndex = std::max(0, errorColumnBox_->findData(columnIndex));
                errorColumnBox_->setCurrentIndex(comboIndex);
            } else {
                errorColumnBox_->setCurrentIndex(0);
            }
        } else {
            errorColumnBox_->setCurrentIndex(0);
        }
    }
}

} // namespace plotapp::ui
