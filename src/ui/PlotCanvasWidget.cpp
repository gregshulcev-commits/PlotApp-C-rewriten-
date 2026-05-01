#include "PlotCanvasWidget.h"

#include "plotapp/LayerSampler.h"

#include <QBuffer>
#include <QByteArray>
#include <QFontMetricsF>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSaveFile>
#include <QTextDocument>
#include <QTextOption>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cctype>

namespace plotapp::ui {
namespace {

double clampValue(double value, double minValue, double maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

QString htmlEscape(const std::string& text) {
    const QString decoded = QString::fromUtf8(text.data(), static_cast<int>(text.size()));
    QString out;
    out.reserve(decoded.size());
    for (const QChar ch : decoded) {
        if (ch == QChar('&')) out += "&amp;";
        else if (ch == QChar('<')) out += "&lt;";
        else if (ch == QChar('>')) out += "&gt;";
        else if (ch == QChar('"')) out += "&quot;";
        else if (ch == QChar('\'')) out += "&#39;";
        else if (ch == QChar('\n')) out += "<br/>";
        else if (ch == QChar('\r')) {
            // Ignore CR from CRLF input; LF will create the line break.
        } else {
            out += ch;
        }
    }
    return out;
}

QString latexLikeToHtml(const std::string& input) {
    QString out;
    const auto greek = [](const std::string& cmd) -> QString {
        if (cmd == "alpha") return QString::fromUtf8("α");
        if (cmd == "beta") return QString::fromUtf8("β");
        if (cmd == "gamma") return QString::fromUtf8("γ");
        if (cmd == "delta") return QString::fromUtf8("δ");
        if (cmd == "Delta") return QString::fromUtf8("Δ");
        if (cmd == "mu") return QString::fromUtf8("μ");
        if (cmd == "sigma") return QString::fromUtf8("σ");
        if (cmd == "Sigma") return QString::fromUtf8("Σ");
        if (cmd == "lambda") return QString::fromUtf8("λ");
        if (cmd == "omega") return QString::fromUtf8("ω");
        if (cmd == "Omega") return QString::fromUtf8("Ω");
        if (cmd == "phi") return QString::fromUtf8("φ");
        if (cmd == "pi") return QString::fromUtf8("π");
        return {};
    };

    std::size_t i = 0;
    while (i < input.size()) {
        const char c = input[i];
        if (c == '\\') {
            std::size_t j = i + 1;
            while (j < input.size() && std::isalpha(static_cast<unsigned char>(input[j]))) ++j;
            const auto cmd = input.substr(i + 1, j - i - 1);
            const auto repl = greek(cmd);
            if (!repl.isEmpty()) out += repl;
            else out += htmlEscape(input.substr(i, std::max<std::size_t>(1, j - i)));
            i = j;
            continue;
        }
        if ((c == '_' || c == '^') && i + 1 < input.size()) {
            const bool isSub = c == '_';
            QString content;
            if (input[i + 1] == '{') {
                std::size_t j = i + 2;
                while (j < input.size() && input[j] != '}') ++j;
                content = htmlEscape(input.substr(i + 2, j - i - 2));
                i = j < input.size() ? j + 1 : j;
            } else {
                content = htmlEscape(input.substr(i + 1, 1));
                i += 2;
            }
            out += isSub ? (QString("<sub>") + content + "</sub>") : (QString("<sup>") + content + "</sup>");
            continue;
        }

        const std::size_t runStart = i;
        while (i < input.size() && input[i] != '\\' && input[i] != '_' && input[i] != '^') ++i;
        out += htmlEscape(input.substr(runStart, i - runStart));
    }
    return out;
}

void drawRichText(QPainter& painter, const QRectF& rect, const QString& html, Qt::Alignment alignment, const QColor& color) {
    QTextDocument doc;
    doc.setDefaultFont(painter.font());
    doc.setDefaultStyleSheet(QString("body, div, p, span, sub, sup { color: %1; }").arg(color.name()));
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setAlignment(alignment);
    doc.setDefaultTextOption(option);
    doc.setHtml(QString("<div style="color:%1;">%2</div>").arg(color.name(), html));
    doc.setTextWidth(rect.width());
    QSizeF size = doc.size();
    QPointF origin = rect.topLeft();
    if (alignment & Qt::AlignHCenter) origin.setX(rect.left() + (rect.width() - size.width()) / 2.0);
    else if (alignment & Qt::AlignRight) origin.setX(rect.right() - size.width());
    if (alignment & Qt::AlignVCenter) origin.setY(rect.top() + (rect.height() - size.height()) / 2.0);
    else if (alignment & Qt::AlignBottom) origin.setY(rect.bottom() - size.height());
    painter.save();
    painter.translate(origin);
    QRectF clip(0, 0, rect.width(), rect.height());
    doc.drawContents(&painter, clip);
    painter.restore();
}



QSizeF richTextSizeForWidth(const QFont& font, const QString& html, double textWidth) {
    QTextDocument doc;
    doc.setDefaultFont(font);
    doc.setHtml(html);
    doc.setTextWidth(std::max(1.0, textWidth));
    return doc.size();
}

QSizeF naturalRichTextSize(const QFont& font, const QString& html) {
    QTextDocument doc;
    doc.setDefaultFont(font);
    doc.setHtml(html);
    doc.setTextWidth(-1);
    const double naturalWidth = std::max(1.0, std::ceil(doc.idealWidth()));
    doc.setTextWidth(naturalWidth);
    return doc.size();
}

bool exportSizeIsSafe(const QSize& size) {
    constexpr qint64 kMaxExportPixels = 64ll * 1024ll * 1024ll;
    if (!size.isValid() || size.width() <= 0 || size.height() <= 0) return false;
    const qint64 pixels = static_cast<qint64>(size.width()) * static_cast<qint64>(size.height());
    return pixels > 0 && pixels <= kMaxExportPixels;
}

double niceStep(double range) {
    if (range <= 0.0) return 1.0;
    const double rough = range / 10.0;
    const double power = std::pow(10.0, std::floor(std::log10(rough)));
    const double scaled = rough / power;
    if (scaled < 1.5) return 1.0 * power;
    if (scaled < 3.5) return 2.0 * power;
    if (scaled < 7.5) return 5.0 * power;
    return 10.0 * power;
}

double snapNearZero(double value, double step) {
    const double threshold = std::max(1e-12, std::abs(step) * 1e-6);
    return std::abs(value) < threshold ? 0.0 : value;
}

QString numberLabel(double value, double step) {
    value = snapNearZero(value, step);
    if (value == 0.0) return "0";
    return QString::number(value, 'g', 6);
}

QColor safeColor(const std::string& value, const QColor& fallback = QColor("#1f77b4")) {
    const QColor color(QString::fromStdString(value));
    return color.isValid() ? color : fallback;
}

bool shouldBreakSegment(const plotapp::Point& previous, const plotapp::Point& current,
                        double xRange, double yRange) {
    return std::fabs(current.y - previous.y) > std::max(1.0, yRange) * 2.0
        || std::fabs(current.x - previous.x) > std::max(1.0, xRange) * 0.25;
}

QColor pointColorFor(const plotapp::Layer& layer, std::size_t sourceIndex) {
    const auto role = plotapp::pointRoleAt(layer, sourceIndex);
    if (role == plotapp::PointRole::Maximum) {
        return safeColor(layer.style.secondaryColor, safeColor(layer.style.color));
    }
    return safeColor(layer.style.color);
}

} // namespace

PlotCanvasWidget::PlotCanvasWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(700, 450);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

QSize PlotCanvasWidget::activeRenderSize() const {
    if (renderSizeOverride_.isValid()) return renderSizeOverride_;
    return size().expandedTo(QSize(32, 32));
}

QRect PlotCanvasWidget::activeRenderRect() const {
    return QRect(QPoint(0, 0), activeRenderSize());
}


void PlotCanvasWidget::setProject(plotapp::Project* project) {
    project_ = project;
    if (project_ && project_->settings().hasCustomViewport) {
        viewXMin_ = project_->settings().viewXMin;
        viewXMax_ = project_->settings().viewXMax;
        viewYMin_ = project_->settings().viewYMin;
        viewYMax_ = project_->settings().viewYMax;
        if (!std::isfinite(viewXMin_) || !std::isfinite(viewXMax_)
            || !std::isfinite(viewYMin_) || !std::isfinite(viewYMax_)) {
            resetViewToProject();
            return;
        }
        if (viewXMin_ > viewXMax_) std::swap(viewXMin_, viewXMax_);
        if (viewYMin_ > viewYMax_) std::swap(viewYMin_, viewYMax_);
        if (viewXMin_ == viewXMax_) {
            viewXMin_ -= 1.0;
            viewXMax_ += 1.0;
        }
        if (viewYMin_ == viewYMax_) {
            viewYMin_ -= 1.0;
            viewYMax_ += 1.0;
        }
    } else {
        resetViewToProject();
    }

    if (project_ == nullptr) {
        selectedLayerId_.clear();
        selectedPointIndices_.clear();
    } else if (const auto* layer = selectedLayer()) {
        std::vector<std::size_t> normalized;
        normalized.reserve(selectedPointIndices_.size());
        std::vector<int> used(layer->points.size(), 0);
        for (const auto index : selectedPointIndices_) {
            if (index < layer->points.size() && used[index] == 0) {
                normalized.push_back(index);
                used[index] = 1;
            }
        }
        selectedPointIndices_ = normalized;
        if (selectedPointIndices_.empty() && !layer->points.empty()) {
            selectedPointIndices_ = fullSelectionForLayer(*layer);
        }
    } else {
        selectedLayerId_.clear();
        selectedPointIndices_.clear();
    }

    emitPointSelectionChanged();
    update();
}

bool PlotCanvasWidget::exportPng(const QString& path) const {
    return exportPng(path, size());
}

bool PlotCanvasWidget::exportPng(const QString& path, const QSize& size, int dpi) const {
    const QImage image = renderToImage(size, dpi);
    if (image.isNull()) return false;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (!image.save(&file, "PNG")) return false;
    return file.commit();
}

bool PlotCanvasWidget::exportSvgSnapshot(const QString& path, const QSize& size, int dpi) const {
    const QImage image = renderToImage(size, dpi);
    if (image.isNull()) return false;

    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) return false;

    const QString svg = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%2\" viewBox=\"0 0 %1 %2\">"
        "<image width=\"%1\" height=\"%2\" preserveAspectRatio=\"none\" href=\"data:image/png;base64,%3\"/>"
        "</svg>")
        .arg(image.width())
        .arg(image.height())
        .arg(QString::fromLatin1(pngBytes.toBase64()));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    if (file.write(svg.toUtf8()) < 0) return false;
    return file.commit();
}

QImage PlotCanvasWidget::renderToImage(const QSize& size, int dpi) const {
    const QSize targetSize = size.expandedTo(QSize(32, 32));
    if (!exportSizeIsSafe(targetSize)) return {};

    // Render the visible canvas at its logical on-screen size, then scale the
    // complete image to the requested export page. Do not set QImage DPI before
    // painting: Qt converts point-sized fonts through the paint device DPI, and
    // high values such as 640 dpi make tick labels and legends explode while the
    // layout margins stay in pixels. DPI is saved as metadata after drawing.
    QSize logicalSize = this->size().expandedTo(QSize(32, 32));
    if (!exportSizeIsSafe(logicalSize)) logicalSize = targetSize;

    QImage logicalImage(logicalSize, QImage::Format_ARGB32_Premultiplied);
    if (logicalImage.isNull()) return {};
    logicalImage.fill(Qt::transparent);

    auto* self = const_cast<PlotCanvasWidget*>(this);
    const QSize previousOverride = self->renderSizeOverride_;
    self->renderSizeOverride_ = logicalSize;

    try {
        QPainter painter(&logicalImage);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        self->paintScene(painter);
    } catch (...) {
        self->renderSizeOverride_ = previousOverride;
        throw;
    }

    self->renderSizeOverride_ = previousOverride;

    QImage image = logicalSize == targetSize
        ? logicalImage
        : logicalImage.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (dpi > 0 && !image.isNull()) {
        const double dotsPerMeter = static_cast<double>(dpi) / 0.0254;
        image.setDotsPerMeterX(static_cast<int>(std::lround(dotsPerMeter)));
        image.setDotsPerMeterY(static_cast<int>(std::lround(dotsPerMeter)));
    }
    return image;
}


void PlotCanvasWidget::setSelectedLayerId(const std::string& layerId) {
    if (layerId.empty()) {
        selectedLayerId_.clear();
        selectedPointIndices_.clear();
        emitPointSelectionChanged();
        update();
        return;
    }

    selectedLayerId_ = layerId;
    selectEntireCurrentLayer();
}

void PlotCanvasWidget::clearSelection() {
    selectingPoints_ = false;
    draggingView_ = false;
    draggingLegend_ = false;
    draggedLegendLayerId_.clear();
    pressedLegendLayerId_.clear();
    selectedLayerId_.clear();
    selectedPointIndices_.clear();
    emitPointSelectionChanged();
    update();
}

bool PlotCanvasWidget::selectionCoversWholeLayer() const {
    const auto* layer = selectedLayer();
    return layer != nullptr && !layer->points.empty() && selectedPointIndices_.size() == layer->points.size();
}

void PlotCanvasWidget::selectEntireCurrentLayer() {
    const auto* layer = selectedLayer();
    if (!layer) {
        selectedLayerId_.clear();
        selectedPointIndices_.clear();
    } else {
        selectedPointIndices_ = fullSelectionForLayer(*layer);
    }
    emitPointSelectionChanged();
    update();
}

const plotapp::Layer* PlotCanvasWidget::selectedLayer() const {
    if (project_ == nullptr || selectedLayerId_.empty()) return nullptr;
    return project_->findLayer(selectedLayerId_);
}

std::vector<std::size_t> PlotCanvasWidget::fullSelectionForLayer(const plotapp::Layer& layer) const {
    std::vector<std::size_t> indices;
    indices.reserve(layer.points.size());
    for (std::size_t i = 0; i < layer.points.size(); ++i) indices.push_back(i);
    return indices;
}

void PlotCanvasWidget::emitPointSelectionChanged() {
    const auto* layer = selectedLayer();
    if (!layer) {
        emit pointSelectionChanged({}, 0, 0, false);
        return;
    }
    emit pointSelectionChanged(QString::fromStdString(layer->id), static_cast<int>(selectedPointIndices_.size()),
                               static_cast<int>(layer->points.size()), selectionCoversWholeLayer());
}

void PlotCanvasWidget::applySelectionRect(const QRectF& pixelRect) {
    const auto* layer = selectedLayer();
    if (layer == nullptr) return;
    if (pixelRect.width() < 3.0 || pixelRect.height() < 3.0) return;

    std::vector<std::size_t> selected;
    selected.reserve(layer->points.size());
    for (std::size_t i = 0; i < layer->points.size(); ++i) {
        if (!plotapp::pointIsVisible(*layer, i)) continue;
        const auto& point = layer->points[i];
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) continue;
        const QPointF mapped = mapPoint(point);
        if (pixelRect.contains(mapped)) selected.push_back(i);
    }
    selectedPointIndices_ = std::move(selected);
    emitPointSelectionChanged();
    update();
}

QRectF PlotCanvasWidget::plotRect() const {
    constexpr double left = 126.0;
    constexpr double top = 48.0;
    constexpr double right = 24.0;
    constexpr double bottom = 88.0;
    const QSize canvasSize = activeRenderSize();
    return QRectF(left, top,
                  std::max(120.0, static_cast<double>(canvasSize.width()) - left - right),
                  std::max(120.0, static_cast<double>(canvasSize.height()) - top - bottom));
}

QRectF PlotCanvasWidget::titleRect() const {
    const QRectF pr = plotRect();
    return QRectF(pr.left(), 8.0, pr.width(), 28.0);
}

QRectF PlotCanvasWidget::yLabelRect() const {
    const QRectF pr = plotRect();
    return QRectF(4.0, pr.top(), 28.0, pr.height());
}

QRectF PlotCanvasWidget::yTickRect() const {
    const QRectF pr = plotRect();
    return QRectF(36.0, pr.top(), 84.0, pr.height());
}

QRectF PlotCanvasWidget::xTickRect() const {
    const QRectF pr = plotRect();
    return QRectF(pr.left(), pr.bottom() + 4.0, pr.width(), 28.0);
}

QRectF PlotCanvasWidget::xLabelRect() const {
    const QRectF pr = plotRect();
    return QRectF(pr.left(), pr.bottom() + 34.0, pr.width(), 24.0);
}

PlotCanvasWidget::Bounds PlotCanvasWidget::dataBounds() const {
    Bounds bounds;
    bool first = true;
    if (!project_) return bounds;
    for (const auto& layer : project_->layers()) {
        if (!layer.visible) continue;
        for (std::size_t i = 0; i < layer.points.size(); ++i) {
            if (!plotapp::pointIsVisible(layer, i)) continue;
            const auto& point = layer.points[i];
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) continue;
            const double halfError = i < layer.errorValues.size() ? std::max(0.0, layer.errorValues[i]) * 0.5 : 0.0;
            const double minY = point.y - halfError;
            const double maxY = point.y + halfError;
            if (first) {
                bounds.minX = bounds.maxX = point.x;
                bounds.minY = minY;
                bounds.maxY = maxY;
                first = false;
            } else {
                bounds.minX = std::min(bounds.minX, point.x);
                bounds.maxX = std::max(bounds.maxX, point.x);
                bounds.minY = std::min(bounds.minY, minY);
                bounds.maxY = std::max(bounds.maxY, maxY);
            }
        }
    }
    if (first) return bounds;
    if (bounds.minX == bounds.maxX) { bounds.minX -= 1.0; bounds.maxX += 1.0; }
    if (bounds.minY == bounds.maxY) { bounds.minY -= 1.0; bounds.maxY += 1.0; }
    return bounds;
}

void PlotCanvasWidget::ensureViewInitialized() {
    if (viewXMin_ == viewXMax_ || viewYMin_ == viewYMax_) resetViewToProject();
}

void PlotCanvasWidget::saveViewportToProject() {
    if (!project_) return;
    project_->settings().hasCustomViewport = true;
    project_->settings().viewXMin = viewXMin_;
    project_->settings().viewXMax = viewXMax_;
    project_->settings().viewYMin = viewYMin_;
    project_->settings().viewYMax = viewYMax_;
}

void PlotCanvasWidget::resetViewToProject() {
    const auto bounds = dataBounds();
    const double xMargin = (bounds.maxX - bounds.minX) * 0.08;
    const double yMargin = (bounds.maxY - bounds.minY) * 0.08;
    viewXMin_ = bounds.minX - (xMargin > 0.0 ? xMargin : 1.0);
    viewXMax_ = bounds.maxX + (xMargin > 0.0 ? xMargin : 1.0);
    viewYMin_ = bounds.minY - (yMargin > 0.0 ? yMargin : 1.0);
    viewYMax_ = bounds.maxY + (yMargin > 0.0 ? yMargin : 1.0);
    saveViewportToProject();
    update();
}

QPointF PlotCanvasWidget::mapPoint(const plotapp::Point& point) const {
    const QRectF pr = plotRect();
    const double x = pr.left() + (point.x - viewXMin_) / (viewXMax_ - viewXMin_) * pr.width();
    const double y = pr.bottom() - (point.y - viewYMin_) / (viewYMax_ - viewYMin_) * pr.height();
    return QPointF(x, y);
}

plotapp::Point PlotCanvasWidget::unmapPoint(const QPointF& pixel) const {
    const QRectF pr = plotRect();
    const double x = viewXMin_ + (pixel.x() - pr.left()) / pr.width() * (viewXMax_ - viewXMin_);
    const double y = viewYMin_ + (pr.bottom() - pixel.y()) / pr.height() * (viewYMax_ - viewYMin_);
    return Point{x, y};
}

void PlotCanvasWidget::zoomAt(const QPointF& pixel, double factorX, double factorY) {
    const auto anchor = unmapPoint(pixel);
    viewXMin_ = anchor.x + (viewXMin_ - anchor.x) * factorX;
    viewXMax_ = anchor.x + (viewXMax_ - anchor.x) * factorX;
    viewYMin_ = anchor.y + (viewYMin_ - anchor.y) * factorY;
    viewYMax_ = anchor.y + (viewYMax_ - anchor.y) * factorY;
    saveViewportToProject();
    update();
}

QRectF PlotCanvasWidget::legendRectForLayer(const plotapp::Layer& layer) const {
    const QRectF pr = plotRect();
    const std::string legendText = layer.legendText.empty() ? layer.name : layer.legendText;
    const QString html = latexLikeToHtml(legendText);
    const double maxLegendWidth = std::max(140.0, pr.width() - 8.0);
    const double swatchAndLeftPadding = 34.0;
    const double rightPadding = 10.0;
    const double verticalPadding = 8.0;
    const double maxTextWidth = std::max(80.0, maxLegendWidth - swatchAndLeftPadding - rightPadding);
    const QSizeF naturalSize = naturalRichTextSize(font(), html);
    const double textWidth = clampValue(std::ceil(naturalSize.width()), 80.0, maxTextWidth);
    const QSizeF wrappedSize = richTextSizeForWidth(font(), html, textWidth);
    const double width = std::min(maxLegendWidth,
                                  std::max(190.0, swatchAndLeftPadding + rightPadding + std::ceil(wrappedSize.width())));
    const double maxLegendHeight = std::max(34.0, pr.height() - 8.0);
    const double height = std::min(maxLegendHeight,
                                   std::max(34.0, verticalPadding + std::ceil(wrappedSize.height())));

    const double x = pr.left() + layer.legendAnchorX * pr.width();
    const double y = pr.top() + layer.legendAnchorY * pr.height();
    return QRectF(x, y, width, height);
}

void PlotCanvasWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    paintScene(painter);
}

void PlotCanvasWidget::paintScene(QPainter& painter) {
    ensureViewInitialized();
    legendRects_.clear();

    const bool dark = project_ != nullptr
        ? project_->settings().uiTheme == "dark"
        : palette().window().color().lightness() < 128;
    const QColor bg = dark ? QColor("#202124") : QColor("#f6f7f9");
    const QColor fg = dark ? QColor("#ffffff") : QColor("#000000");
    const QColor grid = dark ? QColor("#3c4043") : QColor("#d9d9d9");
    const QColor plotBg = dark ? QColor("#111315") : QColor("#ffffff");
    const QColor selectionColor = palette().highlight().color().isValid() ? palette().highlight().color() : QColor("#4285f4");
    painter.fillRect(activeRenderRect(), bg);

    const QRectF pr = plotRect();
    const QRectF tr = titleRect();
    const QRectF yr = yLabelRect();
    const QRectF ytr = yTickRect();
    const QRectF xtr = xTickRect();
    const QRectF xlr = xLabelRect();

    painter.fillRect(pr, plotBg);
    painter.setPen(QPen(fg, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(pr);

    if (!project_) return;

    const double xStep = niceStep(viewXMax_ - viewXMin_);
    const double yStep = niceStep(viewYMax_ - viewYMin_);

    if (project_->settings().showGrid) {
        painter.save();
        painter.setClipRect(pr.adjusted(1, 1, -1, -1));
        painter.setPen(QPen(grid, 1));
        for (double x = std::floor(viewXMin_ / xStep) * xStep; x <= viewXMax_ + xStep * 0.5; x += xStep) {
            const QPointF p = mapPoint(Point{x, viewYMin_});
            painter.drawLine(QPointF(p.x(), pr.top()), QPointF(p.x(), pr.bottom()));
        }
        for (double y = std::floor(viewYMin_ / yStep) * yStep; y <= viewYMax_ + yStep * 0.5; y += yStep) {
            const QPointF p = mapPoint(Point{viewXMin_, y});
            painter.drawLine(QPointF(pr.left(), p.y()), QPointF(pr.right(), p.y()));
        }
        painter.restore();
    }

    const double axisXValue = clampValue(0.0, viewXMin_, viewXMax_);
    const double axisYValue = clampValue(0.0, viewYMin_, viewYMax_);
    const QPointF xAxisLeft = mapPoint(Point{viewXMin_, axisYValue});
    const QPointF xAxisRight = mapPoint(Point{viewXMax_, axisYValue});
    const QPointF yAxisBottom = mapPoint(Point{axisXValue, viewYMin_});
    const QPointF yAxisTop = mapPoint(Point{axisXValue, viewYMax_});
    painter.setPen(QPen(fg, 1.6));
    painter.drawLine(QPointF(pr.left(), xAxisLeft.y()), QPointF(pr.right(), xAxisRight.y()));
    painter.drawLine(QPointF(yAxisBottom.x(), pr.bottom()), QPointF(yAxisTop.x(), pr.top()));

    painter.setPen(fg);
    painter.save();
    painter.setClipRect(xtr);
    QFontMetricsF metrics(painter.font());
    double lastRight = xtr.left() - 1000.0;
    for (double x = std::floor(viewXMin_ / xStep) * xStep; x <= viewXMax_ + xStep * 0.5; x += xStep) {
        const QPointF p = mapPoint(Point{x, viewYMin_});
        const QString textValue = numberLabel(x, xStep);
        const double width = metrics.horizontalAdvance(textValue);
        QRectF labelRect(p.x() - width / 2.0 - 2.0, xtr.top(), width + 4.0, xtr.height());
        labelRect = labelRect.intersected(xtr);
        if (labelRect.left() <= lastRight + 4.0) continue;
        painter.drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, textValue);
        lastRight = labelRect.right();
    }
    painter.restore();

    painter.save();
    painter.setClipRect(ytr);
    double lastBottom = ytr.bottom() + 1000.0;
    for (double y = std::floor(viewYMin_ / yStep) * yStep; y <= viewYMax_ + yStep * 0.5; y += yStep) {
        const QPointF p = mapPoint(Point{viewXMin_, y});
        const QString textValue = numberLabel(y, yStep);
        QRectF labelRect(ytr.left(), p.y() - metrics.height() / 2.0, ytr.width() - 6.0, metrics.height() + 2.0);
        if (labelRect.bottom() >= lastBottom - 2.0) continue;
        painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, textValue);
        lastBottom = labelRect.top();
    }
    painter.restore();

    drawRichText(painter, tr, latexLikeToHtml(project_->settings().title), Qt::AlignCenter, fg);
    drawRichText(painter, xlr, latexLikeToHtml(project_->settings().xLabel), Qt::AlignCenter, fg);
    painter.save();
    painter.translate(yr.center());
    painter.rotate(-90);
    drawRichText(painter, QRectF(-yr.height() / 2.0, -yr.width() / 2.0, yr.height(), yr.width()), latexLikeToHtml(project_->settings().yLabel), Qt::AlignCenter, fg);
    painter.restore();

    const bool wholeSelection = selectionCoversWholeLayer();
    painter.save();
    painter.setClipRect(pr.adjusted(1, 1, -1, -1));
    for (const auto& layer : project_->layers()) {
        if (!layer.visible) continue;
        const auto sampled = plotapp::sampleLayerForViewport(*project_, layer, viewXMin_, viewXMax_, viewYMin_, viewYMax_, static_cast<int>(pr.width() * 1.5));
        if (sampled.points.empty()) continue;

        if (layer.style.connectPoints) {
            painter.setPen(QPen(safeColor(layer.style.color), layer.style.lineWidth));
            painter.setBrush(Qt::NoBrush);
            QPainterPath path;
            bool first = true;
            plotapp::Point previous {};
            bool havePrevious = false;
            const double xRange = viewXMax_ - viewXMin_;
            const double yRange = viewYMax_ - viewYMin_;
            for (const auto& point : sampled.points) {
                if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
                    first = true;
                    havePrevious = false;
                    continue;
                }
                const QPointF mapped = mapPoint(point);
                if (first || (havePrevious && shouldBreakSegment(previous, point, xRange, yRange))) {
                    path.moveTo(mapped);
                    first = false;
                } else {
                    path.lineTo(mapped);
                }
                previous = point;
                havePrevious = true;
            }
            painter.drawPath(path);
        }

        if (!layer.errorValues.empty()) {
            painter.setPen(QPen(safeColor(layer.style.color), std::max(1, layer.style.lineWidth)));
            for (std::size_t i = 0; i < sampled.points.size(); ++i) {
                const std::size_t sourceIndex = i < sampled.sourceIndices.size() ? sampled.sourceIndices[i] : i;
                if (sourceIndex >= layer.errorValues.size()) continue;
                const double half = layer.errorValues[sourceIndex] * 0.5;
                const QPointF top = mapPoint(Point{sampled.points[i].x, sampled.points[i].y + half});
                const QPointF bottom = mapPoint(Point{sampled.points[i].x, sampled.points[i].y - half});
                painter.drawLine(top, bottom);
                painter.drawLine(QPointF(top.x() - 5.0, top.y()), QPointF(top.x() + 5.0, top.y()));
                painter.drawLine(QPointF(bottom.x() - 5.0, bottom.y()), QPointF(bottom.x() + 5.0, bottom.y()));
            }
        }

        if (layer.style.showMarkers) {
            painter.setPen(Qt::NoPen);
            for (std::size_t i = 0; i < sampled.points.size(); ++i) {
                const std::size_t sourceIndex = i < sampled.sourceIndices.size() ? sampled.sourceIndices[i] : i;
                const QColor pointColor = pointColorFor(layer, sourceIndex);
                painter.setBrush(pointColor);
                const QPointF mapped = mapPoint(sampled.points[i]);
                painter.drawEllipse(mapped, 3.5, 3.5);
            }
        }

        const bool isSelectedLayer = !selectedLayerId_.empty() && layer.id == selectedLayerId_;
        if (!isSelectedLayer || selectedPointIndices_.empty()) continue;

        if (wholeSelection && layer.style.connectPoints) {
            painter.setPen(QPen(selectionColor, std::max(3, layer.style.lineWidth + 2), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            QPainterPath selectedPath;
            bool first = true;
            plotapp::Point previous {};
            bool havePrevious = false;
            const double xRange = viewXMax_ - viewXMin_;
            const double yRange = viewYMax_ - viewYMin_;
            for (const auto& point : sampled.points) {
                if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
                    first = true;
                    havePrevious = false;
                    continue;
                }
                const QPointF mapped = mapPoint(point);
                if (first || (havePrevious && shouldBreakSegment(previous, point, xRange, yRange))) {
                    selectedPath.moveTo(mapped);
                    first = false;
                } else {
                    selectedPath.lineTo(mapped);
                }
                previous = point;
                havePrevious = true;
            }
            painter.drawPath(selectedPath);
        }

        const bool drawPointSelection = !wholeSelection || !layer.style.connectPoints
            || (layer.type != LayerType::FormulaSeries && selectedPointIndices_.size() <= 2000);
        if (!drawPointSelection) continue;

        painter.setPen(QPen(selectionColor, 1.8));
        painter.setBrush(Qt::NoBrush);
        const double haloRadius = layer.style.showMarkers ? 6.5 : 5.5;
        for (const auto sourceIndex : selectedPointIndices_) {
            if (sourceIndex >= layer.points.size()) continue;
            if (!plotapp::pointIsVisible(layer, sourceIndex)) continue;
            const auto& point = layer.points[sourceIndex];
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) continue;
            const QPointF mapped = mapPoint(point);
            if (!pr.adjusted(-haloRadius, -haloRadius, haloRadius, haloRadius).contains(mapped)) continue;
            painter.drawEllipse(mapped, haloRadius, haloRadius);
        }
    }
    painter.restore();

    if (selectingPoints_) {
        painter.save();
        const QRectF selectionRect = QRectF(selectionStartPos_, selectionCurrentPos_).normalized().intersected(pr);
        painter.setBrush(QColor(selectionColor.red(), selectionColor.green(), selectionColor.blue(), 40));
        painter.setPen(QPen(selectionColor, 1.0, Qt::DashLine));
        painter.drawRect(selectionRect);
        painter.restore();
    }

    painter.setPen(fg);
    for (const auto& layer : project_->layers()) {
        if (!layer.visible || !layer.legendVisible) continue;
        QRectF box = legendRectForLayer(layer);
        const QRectF allowed = pr.adjusted(4, 4, -4, -4);
        box.moveLeft(clampValue(box.left(), allowed.left(), std::max(allowed.left(), allowed.right() - box.width())));
        box.moveTop(clampValue(box.top(), allowed.top(), std::max(allowed.top(), allowed.bottom() - box.height())));
        legendRects_[layer.id] = box;
        painter.setBrush(QColor(dark ? "#2b2c30" : "#ffffff"));
        painter.setPen(QPen(fg, 1));
        painter.drawRoundedRect(box, 6, 6);
        const QColor primaryColor = safeColor(layer.style.color);
        const QColor secondaryColor = safeColor(layer.style.secondaryColor, primaryColor);
        const double swatchY = box.top() + box.height() / 2.0 - 5.0;
        if (layerSupportsPointRoles(layer) && !layer.pointRoles.empty() && secondaryColor != primaryColor) {
            painter.fillRect(QRectF(box.left() + 8.0, swatchY, 8.0, 10.0), primaryColor);
            painter.fillRect(QRectF(box.left() + 18.0, swatchY, 8.0, 10.0), secondaryColor);
        } else {
            painter.fillRect(QRectF(box.left() + 8.0, swatchY, 18.0, 10.0), primaryColor);
        }
        drawRichText(painter, box.adjusted(32, 4, -6, -4),
                     latexLikeToHtml(layer.legendText.empty() ? layer.name : layer.legendText),
                     Qt::AlignLeft | Qt::AlignVCenter, fg);
    }
}

void PlotCanvasWidget::wheelEvent(QWheelEvent* event) {
    const double factor = event->angleDelta().y() > 0 ? 0.90 : 1.10;
    const auto modifiers = event->modifiers();
    if (modifiers & Qt::ShiftModifier) zoomAt(event->position(), factor, 1.0);
    else if (modifiers & Qt::ControlModifier) zoomAt(event->position(), 1.0, factor);
    else zoomAt(event->position(), factor, factor);
}

void PlotCanvasWidget::keyPressEvent(QKeyEvent* event) {
    if (event != nullptr && event->key() == Qt::Key_Escape) {
        clearSelection();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void PlotCanvasWidget::mousePressEvent(QMouseEvent* event) {
    pressMousePos_ = event->pos();
    lastMousePos_ = event->pos();
    setFocus(Qt::MouseFocusReason);
    if (event->button() != Qt::LeftButton || !project_) return;

    if ((event->modifiers() & Qt::ShiftModifier) && plotRect().contains(event->pos()) && selectedLayer() != nullptr) {
        selectingPoints_ = true;
        selectionStartPos_ = event->pos();
        selectionCurrentPos_ = event->pos();
        draggingView_ = false;
        draggingLegend_ = false;
        draggedLegendLayerId_.clear();
        pressedLegendLayerId_.clear();
        update();
        return;
    }

    pressedLegendLayerId_.clear();
    for (const auto& [layerId, rect] : legendRects_) {
        if (rect.contains(event->pos())) {
            draggingLegend_ = true;
            draggedLegendLayerId_ = layerId;
            pressedLegendLayerId_ = layerId;
            legendDragOffset_ = event->pos() - rect.topLeft();
            return;
        }
    }
    if (plotRect().contains(event->pos())) draggingView_ = true;
}

void PlotCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (selectingPoints_) {
        selectionCurrentPos_ = event->pos();
        update();
        return;
    }
    if (draggingLegend_ && project_) {
        auto* layer = project_->findLayer(draggedLegendLayerId_);
        if (!layer) return;
        const QRectF pr = plotRect();
        const QPointF topLeft = event->pos() - legendDragOffset_;
        const QRectF legendBox = legendRectForLayer(*layer);
        const double maxAnchorX = std::max(0.0, 1.0 - legendBox.width() / std::max(1.0, pr.width()));
        const double maxAnchorY = std::max(0.0, 1.0 - legendBox.height() / std::max(1.0, pr.height()));
        layer->legendAnchorX = clampValue((topLeft.x() - pr.left()) / pr.width(), 0.0, maxAnchorX);
        layer->legendAnchorY = clampValue((topLeft.y() - pr.top()) / pr.height(), 0.0, maxAnchorY);
        update();
        return;
    }
    if (draggingView_) {
        const QPoint delta = event->pos() - lastMousePos_;
        const QRectF pr = plotRect();
        const double dx = -static_cast<double>(delta.x()) / pr.width() * (viewXMax_ - viewXMin_);
        const double dy = static_cast<double>(delta.y()) / pr.height() * (viewYMax_ - viewYMin_);
        viewXMin_ += dx;
        viewXMax_ += dx;
        viewYMin_ += dy;
        viewYMax_ += dy;
        saveViewportToProject();
        lastMousePos_ = event->pos();
        update();
    }
}

void PlotCanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (selectingPoints_) {
        selectionCurrentPos_ = event->pos();
        const QRectF selectionRect = QRectF(selectionStartPos_, selectionCurrentPos_).normalized().intersected(plotRect());
        selectingPoints_ = false;
        applySelectionRect(selectionRect);
        return;
    }

    const bool clicked = (event->pos() - pressMousePos_).manhattanLength() < 4;
    const bool wasDraggingView = draggingView_;
    const bool wasDraggingLegend = draggingLegend_;
    const std::string clickedLegendLayerId = pressedLegendLayerId_;
    draggingView_ = false;
    draggingLegend_ = false;
    draggedLegendLayerId_.clear();
    pressedLegendLayerId_.clear();
    if (!clicked || wasDraggingView) return;
    if (wasDraggingLegend) {
        if (!clickedLegendLayerId.empty()) emit legendClicked(QString::fromStdString(clickedLegendLayerId));
        return;
    }
    if (titleRect().contains(event->pos())) emit titleClicked();
    else if (xLabelRect().contains(event->pos())) emit xLabelClicked();
    else if (yLabelRect().contains(event->pos())) emit yLabelClicked();
}

} // namespace plotapp::ui
