#pragma once

#include "plotapp/Project.h"

#include <QImage>
#include <QKeyEvent>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <map>
#include <string>
#include <vector>

namespace plotapp::ui {

class PlotCanvasWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlotCanvasWidget(QWidget* parent = nullptr);

    void setProject(plotapp::Project* project);
    bool exportPng(const QString& path) const;
    bool exportPng(const QString& path, const QSize& size, int dpi = 0) const;
    QImage renderToImage(const QSize& size, int dpi = 0) const;
    void resetViewToProject();

    double viewXMin() const { return viewXMin_; }
    double viewXMax() const { return viewXMax_; }
    double viewYMin() const { return viewYMin_; }
    double viewYMax() const { return viewYMax_; }

    void setSelectedLayerId(const std::string& layerId);
    std::string selectedLayerId() const { return selectedLayerId_; }
    const std::vector<std::size_t>& selectedPointIndices() const { return selectedPointIndices_; }
    bool selectionCoversWholeLayer() const;
    void selectEntireCurrentLayer();
    void clearSelection();

signals:
    void titleClicked();
    void xLabelClicked();
    void yLabelClicked();
    void pointSelectionChanged(const QString& layerId, int selectedCount, int totalCount, bool wholeLayer);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    struct Bounds {
        double minX {0.0};
        double maxX {1.0};
        double minY {0.0};
        double maxY {1.0};
    };

    QRectF plotRect() const;
    QRectF titleRect() const;
    QRectF yLabelRect() const;
    QRectF yTickRect() const;
    QRectF xTickRect() const;
    QRectF xLabelRect() const;
    Bounds dataBounds() const;
    void ensureViewInitialized();
    void saveViewportToProject();
    QPointF mapPoint(const plotapp::Point& point) const;
    plotapp::Point unmapPoint(const QPointF& pixel) const;
    void zoomAt(const QPointF& pixel, double factorX, double factorY);
    QRectF legendRectForLayer(const plotapp::Layer& layer) const;
    const plotapp::Layer* selectedLayer() const;
    std::vector<std::size_t> fullSelectionForLayer(const plotapp::Layer& layer) const;
    void emitPointSelectionChanged();
    void applySelectionRect(const QRectF& pixelRect);

    plotapp::Project* project_ {nullptr};
    mutable std::map<std::string, QRectF> legendRects_;
    double viewXMin_ {-10.0};
    double viewXMax_ {10.0};
    double viewYMin_ {-10.0};
    double viewYMax_ {10.0};
    bool draggingView_ {false};
    bool draggingLegend_ {false};
    bool selectingPoints_ {false};
    QPoint pressMousePos_;
    QPoint lastMousePos_;
    QPoint selectionStartPos_;
    QPoint selectionCurrentPos_;
    std::string draggedLegendLayerId_;
    QPointF legendDragOffset_;
    std::string selectedLayerId_;
    std::vector<std::size_t> selectedPointIndices_;
};

} // namespace plotapp::ui
