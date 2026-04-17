#pragma once

#include "plotapp/Importer.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QTableWidget;

namespace plotapp::ui {

class ImportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImportDialog(const plotapp::TableData& table, QWidget* parent = nullptr);

    std::size_t xColumn() const;
    std::size_t yColumn() const;
    QString layerName() const;

private slots:
    void updateSuggestedLayerName();
    void onLayerNameEdited(const QString& text);

private:
    QComboBox* xColumnBox_ {nullptr};
    QComboBox* yColumnBox_ {nullptr};
    QLineEdit* layerNameEdit_ {nullptr};
    QTableWidget* previewTable_ {nullptr};
    QString autoSuggestedName_;
    bool autoNameMode_ {true};
};

} // namespace plotapp::ui
