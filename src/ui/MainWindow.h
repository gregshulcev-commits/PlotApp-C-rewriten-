#pragma once

#include "plotapp/CommandDispatcher.h"
#include "plotapp/ProjectController.h"

#include <QFont>
#include <QMainWindow>

class QDockWidget;
class QLineEdit;
class QTextEdit;
class QToolBar;
class QTreeWidget;
class QTreeWidgetItem;

namespace plotapp::ui {

class PlotCanvasWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

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
    void exportPng();
    void exportSvg();
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

    plotapp::ProjectController controller_;
    plotapp::CommandDispatcher dispatcher_;
    PlotCanvasWidget* canvas_ {nullptr};
    QTreeWidget* layerTree_ {nullptr};
    QTextEdit* log_ {nullptr};
    QLineEdit* commandLine_ {nullptr};
    QDockWidget* layerDock_ {nullptr};
    QDockWidget* consoleDock_ {nullptr};
    QToolBar* panelsToolbar_ {nullptr};
    QFont baseFont_;
};

} // namespace plotapp::ui
