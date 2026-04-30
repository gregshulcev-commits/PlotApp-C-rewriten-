#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QSpinBox;

namespace plotapp::ui {

class PlotCanvasWidget;

class ExportDialog : public QDialog {
    Q_OBJECT
public:
    enum class FileFormat {
        Png,
        Svg,
    };

    explicit ExportDialog(const PlotCanvasWidget* canvas, QWidget* parent = nullptr);

    FileFormat format() const;
    int widthPx() const;
    int heightPx() const;
    int dpi() const;
    QString defaultSuffix() const;
    QString fileFilter() const;

private slots:
    void onPresetChanged();
    void onSizeEdited();
    void updatePreview();

private:
    QSize exportSize() const;
    void applyPresetSelection();
    QString presetDescription() const;

    const PlotCanvasWidget* canvas_ {nullptr};
    QComboBox* formatBox_ {nullptr};
    QComboBox* presetBox_ {nullptr};
    QSpinBox* dpiSpin_ {nullptr};
    QSpinBox* widthSpin_ {nullptr};
    QSpinBox* heightSpin_ {nullptr};
    QLabel* summaryLabel_ {nullptr};
    QLabel* previewHintLabel_ {nullptr};
    QLabel* previewLabel_ {nullptr};
    bool applyingPreset_ {false};
};

} // namespace plotapp::ui
