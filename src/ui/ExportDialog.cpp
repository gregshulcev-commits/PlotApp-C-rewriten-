#include "ExportDialog.h"

#include "DialogUtil.h"
#include "PlotCanvasWidget.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>

namespace plotapp::ui {
namespace {

constexpr int kPreviewMaxWidth = 720;
constexpr int kPreviewMaxHeight = 420;

QString sizeLabel(const QSize& size) {
    return QString("%1 x %2 px").arg(size.width()).arg(size.height());
}

} // namespace

ExportDialog::ExportDialog(const PlotCanvasWidget* canvas, QWidget* parent)
    : QDialog(parent), canvas_(canvas) {
    setWindowTitle("Export image");
    applyDialogWindowSize(this, QSize(980, 760), QSize(820, 640));

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    formatBox_ = new QComboBox(this);
    formatBox_->addItem("PNG raster image", static_cast<int>(FileFormat::Png));
    formatBox_->addItem("SVG vector image", static_cast<int>(FileFormat::Svg));

    presetBox_ = new QComboBox(this);
    presetBox_->addItem("Current canvas size", "current");
    presetBox_->addItem("A4 portrait", "a4_portrait");
    presetBox_->addItem("A4 landscape", "a4_landscape");
    presetBox_->addItem("Custom", "custom");

    dpiSpin_ = new QSpinBox(this);
    dpiSpin_->setRange(72, 1200);
    dpiSpin_->setSingleStep(24);
    dpiSpin_->setValue(300);
    dpiSpin_->setSuffix(" dpi");

    widthSpin_ = new QSpinBox(this);
    widthSpin_->setRange(64, 20000);
    widthSpin_->setSingleStep(64);

    heightSpin_ = new QSpinBox(this);
    heightSpin_->setRange(64, 20000);
    heightSpin_->setSingleStep(64);

    form->addRow("Format", formatBox_);
    form->addRow("Canvas preset", presetBox_);
    form->addRow("DPI / print density", dpiSpin_);
    form->addRow("Width", widthSpin_);
    form->addRow("Height", heightSpin_);
    layout->addLayout(form);

    summaryLabel_ = new QLabel(this);
    summaryLabel_->setWordWrap(true);
    layout->addWidget(summaryLabel_);

    previewHintLabel_ = new QLabel(this);
    previewHintLabel_->setWordWrap(true);
    layout->addWidget(previewHintLabel_);

    auto* previewFrame = new QFrame(this);
    previewFrame->setFrameShape(QFrame::StyledPanel);
    auto* previewLayout = new QVBoxLayout(previewFrame);
    previewLabel_ = new QLabel(previewFrame);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setMinimumSize(560, 360);
    previewLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    previewLayout->addWidget(previewLabel_);
    layout->addWidget(previewFrame, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Export...");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(formatBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, &ExportDialog::updatePreview);
    connect(presetBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, &ExportDialog::onPresetChanged);
    connect(dpiSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &ExportDialog::onPresetChanged);
    connect(widthSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &ExportDialog::onSizeEdited);
    connect(heightSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &ExportDialog::onSizeEdited);

    applyPresetSelection();
    updatePreview();
}

ExportDialog::FileFormat ExportDialog::format() const {
    return static_cast<FileFormat>(formatBox_->currentData().toInt());
}

int ExportDialog::widthPx() const {
    return widthSpin_->value();
}

int ExportDialog::heightPx() const {
    return heightSpin_->value();
}

int ExportDialog::dpi() const {
    return dpiSpin_->value();
}

QString ExportDialog::defaultSuffix() const {
    return format() == FileFormat::Svg ? QStringLiteral("svg") : QStringLiteral("png");
}

QString ExportDialog::fileFilter() const {
    return format() == FileFormat::Svg
        ? QStringLiteral("SVG image (*.svg);;All files (*)")
        : QStringLiteral("PNG image (*.png);;All files (*)");
}

QSize ExportDialog::exportSize() const {
    return QSize(widthSpin_->value(), heightSpin_->value());
}

void ExportDialog::onPresetChanged() {
    applyPresetSelection();
    updatePreview();
}

void ExportDialog::onSizeEdited() {
    if (!applyingPreset_ && presetBox_->currentData().toString() != "custom") {
        const int customIndex = presetBox_->findData("custom");
        if (customIndex >= 0) {
            applyingPreset_ = true;
            presetBox_->setCurrentIndex(customIndex);
            applyingPreset_ = false;
        }
    }
    updatePreview();
}

void ExportDialog::applyPresetSelection() {
    if (canvas_ == nullptr) return;
    const QString preset = presetBox_->currentData().toString();
    if (preset == "custom") return;

    QSize targetSize;
    if (preset == "current") {
        targetSize = canvas_->size().expandedTo(QSize(64, 64));
    } else if (preset == "a4_portrait") {
        targetSize = a4PixelsAtDpi(dpiSpin_->value(), false);
    } else if (preset == "a4_landscape") {
        targetSize = a4PixelsAtDpi(dpiSpin_->value(), true);
    }
    if (!targetSize.isValid()) return;

    applyingPreset_ = true;
    widthSpin_->setValue(targetSize.width());
    heightSpin_->setValue(targetSize.height());
    applyingPreset_ = false;
}

QString ExportDialog::presetDescription() const {
    const QString preset = presetBox_->currentData().toString();
    if (preset == "current") return QString("Matches the current on-screen canvas size.");
    if (preset == "a4_portrait") return QString("Fits the plot into an A4 portrait page at the selected DPI.");
    if (preset == "a4_landscape") return QString("Fits the plot into an A4 landscape page at the selected DPI.");
    return QString("Custom export size. Use this when you need exact pixel dimensions.");
}

void ExportDialog::updatePreview() {
    const QSize targetSize = exportSize();
    summaryLabel_->setText(QString("Export canvas: %1. %2")
        .arg(sizeLabel(targetSize), presetDescription()));

    if (format() == FileFormat::Svg) {
        previewHintLabel_->setText("SVG is vector-based. The preview below is a raster approximation of the final composition and aspect ratio.");
    } else {
        previewHintLabel_->setText("PNG is raster-based. The preview below shows the final pixel aspect ratio.");
    }

    if (canvas_ == nullptr || !targetSize.isValid()) {
        previewLabel_->setText("Preview is unavailable.");
        previewLabel_->setPixmap(QPixmap());
        return;
    }

    QSize previewSize = targetSize;
    previewSize.scale(kPreviewMaxWidth, kPreviewMaxHeight, Qt::KeepAspectRatio);
    if (previewSize.width() < 64 || previewSize.height() < 64) {
        previewSize = previewSize.expandedTo(QSize(64, 64));
    }

    const QImage previewImage = canvas_->renderToImage(previewSize);
    previewLabel_->setPixmap(QPixmap::fromImage(previewImage));
}

} // namespace plotapp::ui
