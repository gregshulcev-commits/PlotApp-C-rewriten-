#pragma once

#include "plotapp/Project.h"

#include <QDialog>

class QTableWidget;

namespace plotapp::ui {

class PointsEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit PointsEditorDialog(plotapp::Layer layer, QWidget* parent = nullptr);
    plotapp::Layer result() const;

private:
    void insertRow(int row, const plotapp::Point* point = nullptr, bool visible = true,
                   plotapp::PointRole role = plotapp::PointRole::Normal,
                   const QString& sourceRowText = QString());

    plotapp::Layer layer_;
    QTableWidget* table_ {nullptr};
};

} // namespace plotapp::ui
