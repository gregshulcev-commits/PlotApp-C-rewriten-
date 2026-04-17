#include "PointsEditorDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cmath>

namespace plotapp::ui {
namespace {

constexpr int kColumnX = 0;
constexpr int kColumnY = 1;
constexpr int kColumnVisible = 2;
constexpr int kColumnRole = 3;
constexpr int kColumnSourceRow = 4;

QString roleText(plotapp::PointRole role) {
    switch (role) {
        case plotapp::PointRole::Minimum: return "Minimum";
        case plotapp::PointRole::Maximum: return "Maximum";
        default: return "Normal";
    }
}

plotapp::PointRole roleFromText(const QString& text) {
    if (text == "Minimum") return plotapp::PointRole::Minimum;
    if (text == "Maximum") return plotapp::PointRole::Maximum;
    return plotapp::PointRole::Normal;
}

QComboBox* makeRoleCombo(QWidget* parent, plotapp::PointRole role) {
    auto* combo = new QComboBox(parent);
    combo->addItem("Normal");
    combo->addItem("Minimum");
    combo->addItem("Maximum");
    const int index = combo->findText(roleText(role));
    combo->setCurrentIndex(index >= 0 ? index : 0);
    return combo;
}

QTableWidgetItem* makeCheckItem(bool checked) {
    auto* item = new QTableWidgetItem();
    item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return item;
}

QTableWidgetItem* makeReadOnlyItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

} // namespace

PointsEditorDialog::PointsEditorDialog(plotapp::Layer layer, QWidget* parent)
    : QDialog(parent), layer_(std::move(layer)), roleEditingEnabled_(plotapp::layerSupportsPointRoles(layer_)) {
    setWindowTitle("Edit points");
    auto* layout = new QVBoxLayout(this);
    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"X", "Y", "Visible", "Role", "Source row"});
    table_->setRowCount(0);

    for (std::size_t row = 0; row < layer_.points.size(); ++row) {
        const bool visible = plotapp::pointIsVisible(layer_, row);
        const auto role = plotapp::pointRoleAt(layer_, row);
        QString sourceRowText;
        if (row < layer_.importedRowIndices.size()) {
            sourceRowText = QString::number(static_cast<qulonglong>(layer_.importedRowIndices[row]));
        }
        insertRow(table_->rowCount(), &layer_.points[row], visible, role, sourceRowText);
    }

    table_->setColumnHidden(kColumnRole, !roleEditingEnabled_);

    table_->horizontalHeader()->setSectionResizeMode(kColumnX, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kColumnY, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kColumnVisible, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnRole, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnSourceRow, QHeaderView::ResizeToContents);
    layout->addWidget(table_);

    auto* tools = new QHBoxLayout();
    auto* addRowButton = new QPushButton("Add row", this);
    auto* removeRowButton = new QPushButton("Remove selected row", this);
    tools->addWidget(addRowButton);
    tools->addWidget(removeRowButton);
    tools->addStretch();
    layout->addLayout(tools);
    connect(addRowButton, &QPushButton::clicked, this, [this]() { insertRow(table_->rowCount()); });
    connect(removeRowButton, &QPushButton::clicked, this, [this]() {
        const int row = table_->currentRow();
        if (row >= 0) table_->removeRow(row);
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void PointsEditorDialog::insertRow(int row, const plotapp::Point* point, bool visible,
                                   plotapp::PointRole role, const QString& sourceRowText) {
    table_->insertRow(row);
    table_->setItem(row, kColumnX, new QTableWidgetItem(point ? QString::number(point->x) : QString()));
    table_->setItem(row, kColumnY, new QTableWidgetItem(point ? QString::number(point->y) : QString()));
    table_->setItem(row, kColumnVisible, makeCheckItem(visible));
    if (roleEditingEnabled_) table_->setCellWidget(row, kColumnRole, makeRoleCombo(table_, role));
    else table_->setItem(row, kColumnRole, makeReadOnlyItem({}));
    table_->setItem(row, kColumnSourceRow, makeReadOnlyItem(sourceRowText));
}

plotapp::Layer PointsEditorDialog::result() const {
    auto copy = layer_;
    copy.points.clear();
    copy.pointVisibility.clear();
    copy.pointRoles.clear();

    std::vector<std::size_t> mappedRowIndices;
    mappedRowIndices.reserve(static_cast<std::size_t>(table_->rowCount()));
    bool keepImportedMapping = !layer_.importedRows.empty();

    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* xItem = table_->item(row, kColumnX);
        auto* yItem = table_->item(row, kColumnY);
        auto* visibleItem = table_->item(row, kColumnVisible);
        auto* sourceRowItem = table_->item(row, kColumnSourceRow);
        auto* roleCombo = qobject_cast<QComboBox*>(table_->cellWidget(row, kColumnRole));
        if (!xItem || !yItem || !visibleItem) continue;

        bool okX = false;
        bool okY = false;
        const double x = xItem->text().toDouble(&okX);
        const double y = yItem->text().toDouble(&okY);
        if (!okX || !okY || !std::isfinite(x) || !std::isfinite(y)) continue;

        copy.points.push_back(plotapp::Point{x, y});
        copy.pointVisibility.push_back(visibleItem->checkState() == Qt::Checked ? 1 : 0);
        if (roleEditingEnabled_) {
            const auto role = roleCombo != nullptr ? roleFromText(roleCombo->currentText()) : plotapp::PointRole::Normal;
            copy.pointRoles.push_back(static_cast<int>(role));
        }

        if (keepImportedMapping) {
            bool okRow = false;
            const qulonglong sourceRow = sourceRowItem ? sourceRowItem->text().toULongLong(&okRow) : 0;
            if (!okRow) {
                keepImportedMapping = false;
            } else {
                mappedRowIndices.push_back(static_cast<std::size_t>(sourceRow));
            }
        }
    }

    if (!roleEditingEnabled_) {
        copy.pointRoles.clear();
    }

    if (keepImportedMapping && mappedRowIndices.size() == copy.points.size()) {
        copy.importedRowIndices = std::move(mappedRowIndices);
    } else if (!copy.importedRows.empty()) {
        copy.importedSourcePath.clear();
        copy.importedSheetName.clear();
        copy.importedHeaders.clear();
        copy.importedRows.clear();
        copy.importedRowIndices.clear();
        copy.importedXColumn = 0;
        copy.importedYColumn = 1;
    }

    return copy;
}

} // namespace plotapp::ui
