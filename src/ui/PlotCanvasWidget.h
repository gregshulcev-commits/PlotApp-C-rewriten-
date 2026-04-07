#pragma once

#include "plotapp/Project.h"

#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QWidget>

#include <map>
#include <string>

namespace plotapp::ui {

class PlotCanvasWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlotCanvasWidget(QWidget* parent = nullptr);

    void setProject(plotapp::Project* project);
    bool exportPng(const QString& path) const;
    void resetViewToProject();

signals:
    void titleClicked();
    void xLabelClicked();
    void yLabelClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
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

    plotapp::Project* project_ {nullptr};
    mutable std::map<std::string, QRectF> legendRects_;
    double viewXMin_ {-10.0};
    double viewXMax_ {10.0};
    double viewYMin_ {-10.0};
    double viewYMax_ {10.0};
    bool draggingView_ {false};
    bool draggingLegend_ {false};
    QPoint pressMousePos_;
    QPoint lastMousePos_;
    std::string draggedLegendLayerId_;
    QPointF legendDragOffset_;
};

} // namespace plotapp::ui
