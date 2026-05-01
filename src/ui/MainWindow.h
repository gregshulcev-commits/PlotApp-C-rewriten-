#pragma once

#include "plotapp/CommandDispatcher.h"
#include "plotapp/ProjectController.h"

#include <QFont>
#include <QMainWindow>
#include <QString>

class QCloseEvent;
class QDockWidget;
class QEvent;
class QGridLayout;
class QLineEdit;
class QTextEdit;
class QTimer;
class QWidget;
class QToolBar;
class QTreeWidget;
class QTreeWidgetItem;

namespace plotapp::ui {

class PlotCanvasWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr, bool restoreAutosave = true);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void refreshLayers();
    void importData();
    void addManualLayer();
    void addFormulaLayer();
    void editProjectMetadata();
    void editProjectTitleInline();
    void editXAxisInline();
    void editYAxisInline();
    void saveProject();
    void openProject();
    void exportImage();
    void openSettingsDialog();
    void runSelectedPlugin();
    void editSelectedLayer();
    void removeSelectedLayer();
    void executeConsoleCommand();
    void completeConsoleCommand();
    void onLayerItemChanged(QTreeWidgetItem* item, int column);
    void onCurrentLayerChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onLayerItemClicked(QTreeWidgetItem* item, int column);
    void onPointSelectionChanged(const QString& layerId, int selectedCount, int totalCount, bool wholeLayer);
    void onLayerDoubleClicked(QTreeWidgetItem* item, int column);
    void applyThemeLight();
    void applyThemeDark();
    void setUiScale100();
    void setUiScale115();
    void setUiScale130();
    void resetView();
    void clearCurrentSelection();
    void newProjectWindow();
    void fitCanvasA4Landscape();
    void fitCanvasA4Portrait();
    void releaseCanvasAspect();
    void editLegendInline(const QString& layerId);
    void autosaveProject();

private:
    QString selectedLayerId() const;
    void appendLog(const QString& text);
    void buildMenus();
    void buildDocks();
    void applyTheme(const QString& theme);
    void applyUiScale(int percent);
    void configureFloatingDock(QDockWidget* dock);
    void restoreDock(QDockWidget* dock, Qt::DockWidgetArea area);
    void restorePanelsDefaultLayout();
    void updateCanvasAspectConstraint();
    QString autosaveFilePath() const;
    bool restoreAutosaveIfPresent();
    bool projectHasUserContent() const;
    void startAutosave();

    plotapp::ProjectController controller_;
    plotapp::CommandDispatcher dispatcher_;
    QWidget* canvasHost_ {nullptr};
    QGridLayout* canvasHostLayout_ {nullptr};
    PlotCanvasWidget* canvas_ {nullptr};
    QTreeWidget* layerTree_ {nullptr};
    QTextEdit* log_ {nullptr};
    QLineEdit* commandLine_ {nullptr};
    QDockWidget* layerDock_ {nullptr};
    QDockWidget* consoleDock_ {nullptr};
    QToolBar* panelsToolbar_ {nullptr};
    QToolBar* canvasToolbar_ {nullptr};
    QTimer* autosaveTimer_ {nullptr};
    QFont baseFont_;
    double lockedCanvasAspect_ {0.0};
};

} // namespace plotapp::ui
