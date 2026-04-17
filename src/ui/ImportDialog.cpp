#include "ImportDialog.h"

#include "DialogUtil.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace plotapp::ui {
namespace {

QString headerLabelFor(const plotapp::TableData& table, int index) {
    if (index >= 0 && static_cast<std::size_t>(index) < table.headers.size()) {
        const auto& header = table.headers[static_cast<std::size_t>(index)];
        if (!header.empty()) return QString::fromStdString(header);
    }
    return QString("Column %1").arg(index + 1);
}

QString importedLayerDisplayName(const plotapp::TableData& table, int xIndex, int yIndex) {
    const QString xLabel = headerLabelFor(table, xIndex).trimmed();
    const QString yLabel = headerLabelFor(table, yIndex).trimmed();
    if (!xLabel.isEmpty() && !yLabel.isEmpty() && xLabel != yLabel) return QString("%1 vs %2").arg(yLabel, xLabel);
    if (!yLabel.isEmpty()) return yLabel;
    if (!xLabel.isEmpty()) return xLabel;
    return QString::fromStdString(table.sourcePath.empty() ? "Imported layer" : table.sourcePath);
}

} // namespace

ImportDialog::ImportDialog(const plotapp::TableData& table, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Import columns");
    applyDialogWindowSize(this, QSize(980, 620), QSize(760, 500));
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QString("Source: %1").arg(QString::fromStdString(table.sourcePath))));
    if (!table.sheetName.empty()) {
        layout->addWidget(new QLabel(QString("Sheet: %1").arg(QString::fromStdString(table.sheetName))));
    }

    layerNameEdit_ = new QLineEdit(this);
    layerNameEdit_->setPlaceholderText("Layer name");
    layout->addWidget(layerNameEdit_);

    xColumnBox_ = new QComboBox(this);
    yColumnBox_ = new QComboBox(this);
    for (int index = 0; index < static_cast<int>(table.headers.size()); ++index) {
        const auto& header = table.headers[static_cast<std::size_t>(index)];
        const QString label = QString::fromStdString(header);
        xColumnBox_->addItem(label, index);
        yColumnBox_->addItem(label, index);
    }
    layout->addWidget(new QLabel("X column"));
    layout->addWidget(xColumnBox_);
    layout->addWidget(new QLabel("Y column"));
    layout->addWidget(yColumnBox_);
    layout->addWidget(new QLabel("Errors are added later as a separate plugin/layer."));
    if (table.headers.size() > 1) yColumnBox_->setCurrentIndex(1);
    updateSuggestedLayerName();
    connect(xColumnBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, &ImportDialog::updateSuggestedLayerName);
    connect(yColumnBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, &ImportDialog::updateSuggestedLayerName);
    connect(layerNameEdit_, &QLineEdit::textEdited, this, &ImportDialog::onLayerNameEdited);

    previewTable_ = new QTableWidget(this);
    previewTable_->setColumnCount(static_cast<int>(table.headers.size()));
    QStringList headerLabels;
    for (const auto& header : table.headers) headerLabels << QString::fromStdString(header);
    previewTable_->setHorizontalHeaderLabels(headerLabels);
    previewTable_->setRowCount(static_cast<int>(std::min<std::size_t>(table.rows.size(), 12)));
    for (int row = 0; row < previewTable_->rowCount(); ++row) {
        for (int col = 0; col < previewTable_->columnCount(); ++col) {
            QString value;
            if (static_cast<std::size_t>(row) < table.rows.size() && static_cast<std::size_t>(col) < table.rows[row].size()) {
                value = QString::fromStdString(table.rows[row][col]);
            }
            previewTable_->setItem(row, col, new QTableWidgetItem(value));
        }
    }
    previewTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    previewTable_->setMinimumHeight(320);
    layout->addWidget(previewTable_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

std::size_t ImportDialog::xColumn() const { return static_cast<std::size_t>(xColumnBox_->currentData().toInt()); }
std::size_t ImportDialog::yColumn() const { return static_cast<std::size_t>(yColumnBox_->currentData().toInt()); }
QString ImportDialog::layerName() const { return layerNameEdit_->text(); }

void ImportDialog::updateSuggestedLayerName() {
    const int xIndex = xColumnBox_ != nullptr ? xColumnBox_->currentData().toInt() : 0;
    const int yIndex = yColumnBox_ != nullptr ? yColumnBox_->currentData().toInt() : 0;

    plotapp::TableData table;
    table.sourcePath = {};
    table.headers.reserve(static_cast<std::size_t>(std::max(xIndex, yIndex) + 1));
    for (int i = 0; i < xColumnBox_->count(); ++i) {
        table.headers.push_back(xColumnBox_->itemText(i).toStdString());
    }

    const QString suggested = importedLayerDisplayName(table, xIndex, yIndex);
    const bool shouldApply = autoNameMode_ || layerNameEdit_->text().trimmed().isEmpty() || layerNameEdit_->text() == autoSuggestedName_;
    autoSuggestedName_ = suggested;
    if (!shouldApply) return;

    QSignalBlocker blocker(layerNameEdit_);
    layerNameEdit_->setText(suggested);
    autoNameMode_ = true;
}

void ImportDialog::onLayerNameEdited(const QString& text) {
    autoNameMode_ = text.trimmed().isEmpty() || text == autoSuggestedName_;
}

} // namespace plotapp::ui
