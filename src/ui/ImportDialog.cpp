#include "ImportDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace plotapp::ui {

ImportDialog::ImportDialog(const plotapp::TableData& table, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Import columns");
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
    layout->addWidget(previewTable_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

std::size_t ImportDialog::xColumn() const { return static_cast<std::size_t>(xColumnBox_->currentData().toInt()); }
std::size_t ImportDialog::yColumn() const { return static_cast<std::size_t>(yColumnBox_->currentData().toInt()); }
QString ImportDialog::layerName() const { return layerNameEdit_->text(); }

} // namespace plotapp::ui
