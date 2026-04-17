#include "MainWindow.h"

#include "FormulaLayerDialog.h"
#include "ImportDialog.h"
#include "LayerPropertiesDialog.h"
#include "PlotCanvasWidget.h"
#include "PluginRunDialog.h"
#include "PointsEditorDialog.h"
#include "SettingsDialog.h"
#include "TextEntryDialog.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCompleter>
#include <QDockWidget>
#include <QFileDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QStatusBar>
#include <QStringListModel>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <functional>
#include <map>
#include <string>

namespace plotapp::ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), dispatcher_(controller_), baseFont_(qApp->font()) {
    setWindowTitle("PlotApp Modular UI");
    resize(1400, 900);
    setDockNestingEnabled(true);
    setDockOptions(AllowNestedDocks | AllowTabbedDocks | AnimatedDocks | GroupedDragging);

    canvas_ = new PlotCanvasWidget(this);
    canvas_->setProject(&controller_.project());
    setCentralWidget(canvas_);
    connect(canvas_, &PlotCanvasWidget::titleClicked, this, &MainWindow::editProjectTitleInline);
    connect(canvas_, &PlotCanvasWidget::xLabelClicked, this, &MainWindow::editXAxisInline);
    connect(canvas_, &PlotCanvasWidget::yLabelClicked, this, &MainWindow::editYAxisInline);
    connect(canvas_, &PlotCanvasWidget::pointSelectionChanged, this, &MainWindow::onPointSelectionChanged);

    buildDocks();
    buildMenus();
    refreshLayers();
    applyTheme(QString::fromStdString(controller_.project().settings().uiTheme));
    appendLog("Application started.");
}

void MainWindow::buildMenus() {
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("New project", [this]() {
        controller_.reset();
        canvas_->setProject(&controller_.project());
        refreshLayers();
        applyTheme(QString::fromStdString(controller_.project().settings().uiTheme));
        applyUiScale(controller_.project().settings().uiFontPercent);
        canvas_->resetViewToProject();
    });
    fileMenu->addAction("Open project...", this, &MainWindow::openProject);
    auto* saveAction = fileMenu->addAction("Save project...", this, &MainWindow::saveProject);
    saveAction->setShortcut(QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction("Import data...", this, &MainWindow::importData);
    fileMenu->addSeparator();
    fileMenu->addAction("Export PNG...", this, &MainWindow::exportPng);
    fileMenu->addAction("Export SVG...", this, &MainWindow::exportSvg);
    fileMenu->addSeparator();
    fileMenu->addAction("Settings...", this, &MainWindow::openSettingsDialog);
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", this, &QWidget::close);

    auto* projectMenu = menuBar()->addMenu("&Project");
    projectMenu->addAction("Edit title and axis labels...", this, &MainWindow::editProjectMetadata);

    auto* layerMenu = menuBar()->addMenu("&Layer");
    layerMenu->addAction("Add empty layer...", this, &MainWindow::addManualLayer);
    layerMenu->addAction("Add formula layer...", this, &MainWindow::addFormulaLayer);
    layerMenu->addSeparator();
    layerMenu->addAction("Edit selected layer...", this, &MainWindow::editSelectedLayer);
    layerMenu->addAction("Remove selected layer", this, &MainWindow::removeSelectedLayer);

    auto* pluginMenu = menuBar()->addMenu("&Plugins");
    pluginMenu->addAction("Apply plugin to selected layer...", this, &MainWindow::runSelectedPlugin);
    pluginMenu->addAction("Reload plugins", [this]() {
        controller_.pluginManager().discover();
        appendLog("Plugins reloaded.");
        statusBar()->showMessage("Plugins reloaded", 2000);
    });

    auto* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("Reset view to data", this, &MainWindow::resetView);
    viewMenu->addAction("Restore default panel layout", [this]() { restorePanelsDefaultLayout(); });
    viewMenu->addSeparator();
    if (layerDock_ != nullptr) viewMenu->addAction(layerDock_->toggleViewAction());
    if (consoleDock_ != nullptr) viewMenu->addAction(consoleDock_->toggleViewAction());
    viewMenu->addSeparator();
    auto* themeMenu = viewMenu->addMenu("Theme");
    themeMenu->addAction("Light", this, &MainWindow::applyThemeLight);
    themeMenu->addAction("Dark", this, &MainWindow::applyThemeDark);
    auto* scaleMenu = viewMenu->addMenu("UI scale");
    scaleMenu->addAction("100%", this, &MainWindow::setUiScale100);
    scaleMenu->addAction("115%", this, &MainWindow::setUiScale115);
    scaleMenu->addAction("130%", this, &MainWindow::setUiScale130);

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("Show command help", [this]() { appendLog(QString::fromStdString(dispatcher_.help())); });
}

void MainWindow::configureFloatingDock(QDockWidget* dock) {
    if (dock == nullptr) return;
    dock->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(dock, &QDockWidget::topLevelChanged, this, [dock](bool floating) {
        if (floating) {
            dock->setAllowedAreas(Qt::NoDockWidgetArea);
            dock->setWindowFlag(Qt::Tool, false);
            dock->setWindowFlag(Qt::Window, true);
            dock->setWindowFlag(Qt::WindowStaysOnTopHint, false);
            dock->resize(dock->size().expandedTo(QSize(340, 240)));
        } else {
            dock->setAllowedAreas(Qt::AllDockWidgetAreas);
            dock->setWindowFlag(Qt::Tool, false);
            dock->setWindowFlag(Qt::WindowStaysOnTopHint, false);
        }
        dock->show();
    });
}

void MainWindow::restoreDock(QDockWidget* dock, Qt::DockWidgetArea area) {
    if (dock == nullptr) return;
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    if (dock->isFloating()) dock->setFloating(false);
    addDockWidget(area, dock);
    dock->show();
    dock->raise();
}

void MainWindow::restorePanelsDefaultLayout() {
    restoreDock(layerDock_, Qt::LeftDockWidgetArea);
    restoreDock(consoleDock_, Qt::BottomDockWidgetArea);
    if (layerDock_ != nullptr && consoleDock_ != nullptr) {
        resizeDocks({layerDock_, consoleDock_}, {320, 260}, Qt::Horizontal);
    }
}

void MainWindow::buildDocks() {
    layerDock_ = new QDockWidget("Layers", this);
    layerDock_->setAllowedAreas(Qt::AllDockWidgetAreas);
    layerDock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    layerTree_ = new QTreeWidget(layerDock_);
    layerTree_->setHeaderLabels({"Layer", "Type"});
    layerTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    layerTree_->setRootIsDecorated(true);
    layerDock_->setWidget(layerTree_);
    addDockWidget(Qt::LeftDockWidgetArea, layerDock_);
    configureFloatingDock(layerDock_);
    connect(layerTree_, &QTreeWidget::itemChanged, this, &MainWindow::onLayerItemChanged);
    connect(layerTree_, &QTreeWidget::currentItemChanged, this, &MainWindow::onCurrentLayerChanged);
    connect(layerTree_, &QTreeWidget::itemClicked, this, &MainWindow::onLayerItemClicked);
    connect(layerTree_, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onLayerDoubleClicked);

    consoleDock_ = new QDockWidget("Command console", this);
    consoleDock_->setAllowedAreas(Qt::AllDockWidgetAreas);
    consoleDock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    auto* consoleWidget = new QWidget(consoleDock_);
    auto* consoleLayout = new QVBoxLayout(consoleWidget);
    log_ = new QTextEdit(consoleWidget);
    log_->setReadOnly(true);
    commandLine_ = new QLineEdit(consoleWidget);
    commandLine_->setPlaceholderText("Type a PlotApp command or cd/ls. Shell commands require PLOTAPP_ENABLE_SHELL=1");
    auto* completer = new QCompleter(this);
    QStringList commandNames;
    for (const auto& name : plotapp::CommandDispatcher::builtinCommands()) commandNames << QString::fromStdString(name);
    completer->setModel(new QStringListModel(commandNames, completer));
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::InlineCompletion);
    commandLine_->setCompleter(completer);
    consoleLayout->addWidget(log_);
    consoleLayout->addWidget(commandLine_);
    consoleDock_->setWidget(consoleWidget);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock_);
    configureFloatingDock(consoleDock_);
    connect(commandLine_, &QLineEdit::returnPressed, this, &MainWindow::executeConsoleCommand);
    auto* tabShortcut = new QShortcut(QKeySequence(Qt::Key_Tab), commandLine_);
    connect(tabShortcut, &QShortcut::activated, this, &MainWindow::completeConsoleCommand);

    panelsToolbar_ = addToolBar("Panels");
    panelsToolbar_->setObjectName("PanelsToolbar");
    panelsToolbar_->setFloatable(true);
    panelsToolbar_->addAction(layerDock_->toggleViewAction());
    panelsToolbar_->addAction(consoleDock_->toggleViewAction());
    panelsToolbar_->addSeparator();
    panelsToolbar_->addAction("Reset panel layout", [this]() { restorePanelsDefaultLayout(); });
}

void MainWindow::refreshLayers() {
    if (layerTree_ == nullptr) return;
    const QString previousSelectedId = selectedLayerId();

    layerTree_->blockSignals(true);
    layerTree_->clear();
    std::map<std::string, QTreeWidgetItem*> items;
    for (const auto& layer : controller_.project().layers()) {
        auto* item = new QTreeWidgetItem();
        item->setText(0, QString::fromStdString(layer.name));
        item->setText(1, QString::fromStdString(layerTypeToString(layer.type)));
        item->setData(0, Qt::UserRole, QString::fromStdString(layer.id));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, layer.visible ? Qt::Checked : Qt::Unchecked);
        QString details = QString("id=%1\nplugin=%2\npoints=%3")
            .arg(QString::fromStdString(layer.id))
            .arg(QString::fromStdString(layer.generatorPluginId))
            .arg(static_cast<int>(layer.points.size()));
        if (!layer.errorValues.empty()) details += QString("\nerrors=%1").arg(static_cast<int>(layer.errorValues.size()));
        if (!layer.pointVisibility.empty()) {
            int hiddenCount = 0;
            for (int visible : layer.pointVisibility) if (visible == 0) ++hiddenCount;
            if (hiddenCount > 0) details += QString("\nhidden points=%1").arg(hiddenCount);
        }
        if (!layer.pluginSourcePointIndices.empty()) {
            details += QString("\nselected source points=%1").arg(static_cast<int>(layer.pluginSourcePointIndices.size()));
        }
        item->setToolTip(0, details);
        items[layer.id] = item;
    }
    for (const auto& layer : controller_.project().layers()) {
        auto* item = items[layer.id];
        const std::string parentId = !layer.parentLayerId.empty() ? layer.parentLayerId : layer.sourceLayerId;
        if (!parentId.empty() && items.count(parentId) > 0) {
            items[parentId]->addChild(item);
        } else {
            layerTree_->addTopLevelItem(item);
        }
    }
    layerTree_->expandAll();

    QTreeWidgetItem* restoredItem = nullptr;
    if (!previousSelectedId.isEmpty()) {
        const auto it = items.find(previousSelectedId.toStdString());
        if (it != items.end()) restoredItem = it->second;
    }
    layerTree_->setCurrentItem(restoredItem);
    layerTree_->blockSignals(false);

    if (restoredItem != nullptr) {
        canvas_->setSelectedLayerId(restoredItem->data(0, Qt::UserRole).toString().toStdString());
    } else {
        canvas_->setSelectedLayerId(std::string{});
    }
    canvas_->update();
}

QString MainWindow::selectedLayerId() const {
    auto* item = layerTree_ != nullptr ? layerTree_->currentItem() : nullptr;
    return item ? item->data(0, Qt::UserRole).toString() : QString();
}

void MainWindow::appendLog(const QString& text) {
    if (log_ != nullptr) log_->append(text);
}

void MainWindow::importData() {
    const auto path = QFileDialog::getOpenFileName(this, "Import data", {}, "Data files (*.csv *.txt *.dat *.xlsx);;All files (*)");
    if (path.isEmpty()) return;
    try {
        const auto table = controller_.previewFile(path.toStdString());
        ImportDialog dialog(table, this);
        if (dialog.exec() != QDialog::Accepted) return;
        controller_.importLayer(path.toStdString(), dialog.xColumn(), dialog.yColumn(), dialog.layerName().toStdString());
        canvas_->resetViewToProject();
        refreshLayers();
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Import error", ex.what());
    }
}

void MainWindow::addManualLayer() {
    TextEntryDialog dialog("Add layer", "Layer name", "Layer", this);
    if (dialog.exec() != QDialog::Accepted) return;
    controller_.createManualLayer(dialog.text().toStdString());
    refreshLayers();
}

void MainWindow::addFormulaLayer() {
    const bool hadLayers = !controller_.project().layers().empty();
    FormulaLayerDialog dialog(this);
    if (hadLayers) dialog.setSuggestedRange(canvas_->viewXMin(), canvas_->viewXMax());
    if (dialog.exec() != QDialog::Accepted) return;
    try {
        controller_.createFormulaLayer(dialog.layerName().toStdString(), dialog.expression().toStdString(), dialog.xMin(), dialog.xMax(), dialog.samples());
        if (!hadLayers) canvas_->resetViewToProject();
        else canvas_->update();
        refreshLayers();
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Formula error", ex.what());
    }
}

void MainWindow::editProjectMetadata() {
    editProjectTitleInline();
    editXAxisInline();
    editYAxisInline();
    canvas_->update();
}

void MainWindow::editProjectTitleInline() {
    TextEntryDialog dialog("Project title", "Title", QString::fromStdString(controller_.project().settings().title), this);
    if (dialog.exec() != QDialog::Accepted) return;
    controller_.project().settings().title = dialog.text().toStdString();
    canvas_->update();
}

void MainWindow::editXAxisInline() {
    TextEntryDialog dialog("X axis label", "Label", QString::fromStdString(controller_.project().settings().xLabel), this);
    if (dialog.exec() != QDialog::Accepted) return;
    controller_.project().settings().xLabel = dialog.text().toStdString();
    canvas_->update();
}

void MainWindow::editYAxisInline() {
    TextEntryDialog dialog("Y axis label", "Label", QString::fromStdString(controller_.project().settings().yLabel), this);
    if (dialog.exec() != QDialog::Accepted) return;
    controller_.project().settings().yLabel = dialog.text().toStdString();
    canvas_->update();
}

void MainWindow::saveProject() {
    auto path = QFileDialog::getSaveFileName(this, "Save project", {}, "PlotApp project (*.plotapp);;Text project (*.txt);;All files (*)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".plotapp") && !path.endsWith(".txt")) path += ".plotapp";
    try {
        controller_.saveProject(path.toStdString());
        statusBar()->showMessage("Project saved", 2000);
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Save error", ex.what());
    }
}

void MainWindow::openProject() {
    const auto path = QFileDialog::getOpenFileName(this, "Open project", {}, "Project files (*.plotapp *.txt);;All files (*)");
    if (path.isEmpty()) return;
    try {
        controller_.openProject(path.toStdString());
        canvas_->setProject(&controller_.project());
        const auto warnings = controller_.recomputeDerivedLayers();
        refreshLayers();
        applyTheme(QString::fromStdString(controller_.project().settings().uiTheme));
        applyUiScale(controller_.project().settings().uiFontPercent);
        for (const auto& warning : warnings) appendLog(QString::fromStdString(warning));
        statusBar()->showMessage("Project opened", 2000);
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Open error", ex.what());
    }
}

void MainWindow::exportPng() {
    auto path = QFileDialog::getSaveFileName(this, "Export PNG", {}, "PNG image (*.png);;All files (*)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".png")) path += ".png";
    if (!canvas_->exportPng(path)) {
        QMessageBox::warning(this, "Export PNG", "Failed to save PNG");
    }
}

void MainWindow::exportSvg() {
    auto path = QFileDialog::getSaveFileName(this, "Export SVG", {}, "SVG image (*.svg);;All files (*)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".svg")) path += ".svg";
    try {
        controller_.exportSvg(path.toStdString());
    } catch (const std::exception& ex) {
        QMessageBox::warning(this, "Export SVG", ex.what());
    }
}

void MainWindow::openSettingsDialog() {
    SettingsDialog dialog(QString::fromStdString(controller_.project().settings().uiTheme), controller_.project().settings().uiFontPercent, this);
    if (dialog.exec() != QDialog::Accepted) return;
    applyTheme(dialog.theme());
    applyUiScale(dialog.scalePercent());
}

void MainWindow::runSelectedPlugin() {
    controller_.pluginManager().discover();
    if (controller_.pluginManager().plugins().empty()) {
        QMessageBox::information(this, "Plugins", "No plugins found.");
        return;
    }
    const auto layerId = selectedLayerId();
    if (layerId.isEmpty()) {
        QMessageBox::information(this, "Apply plugin", "Select a source layer first.");
        return;
    }
    const auto* sourceLayer = controller_.project().findLayer(layerId.toStdString());
    if (sourceLayer == nullptr) {
        QMessageBox::information(this, "Apply plugin", "Selected layer is no longer available.");
        return;
    }

    const auto& selectedPointIndices = canvas_->selectedPointIndices();
    if (selectedPointIndices.empty()) {
        QMessageBox::information(this, "Apply plugin", "No points are selected on the chosen source layer.");
        return;
    }

    PluginRunDialog dialog(controller_.pluginManager().plugins(), sourceLayer, this);
    if (dialog.exec() != QDialog::Accepted) return;
    try {
        controller_.applyPlugin(dialog.pluginId().toStdString(), layerId.toStdString(), dialog.params().toStdString(), selectedPointIndices);
        refreshLayers();
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Plugin error", ex.what());
    }
}

void MainWindow::editSelectedLayer() {
    const auto layerId = selectedLayerId();
    if (layerId.isEmpty()) return;
    auto* layer = controller_.project().findLayer(layerId.toStdString());
    if (layer == nullptr) return;

    LayerPropertiesDialog dialog(*layer, this);
    if (dialog.exec() != QDialog::Accepted) return;
    if (dialog.deleteRequested()) {
        controller_.project().removeLayer(layerId.toStdString());
        refreshLayers();
        return;
    }

    auto edited = dialog.result();
    if (edited.type != LayerType::FormulaSeries) {
        PointsEditorDialog pointEditor(edited, this);
        if (pointEditor.exec() == QDialog::Accepted) {
            edited = pointEditor.result();
        }
    }

    if (edited.type == LayerType::FormulaSeries && !edited.formulaExpression.empty()) {
        try {
            controller_.regenerateFormulaLayer(edited);
        } catch (const std::exception& ex) {
            QMessageBox::warning(this, "Formula update", ex.what());
            return;
        }
    }

    *layer = edited;
    if (layer->legendText.empty()) layer->legendText = layer->name;
    refreshLayers();
}

void MainWindow::removeSelectedLayer() {
    const auto layerId = selectedLayerId();
    if (layerId.isEmpty()) return;
    controller_.project().removeLayer(layerId.toStdString());
    refreshLayers();
}

void MainWindow::executeConsoleCommand() {
    const auto command = commandLine_->text();
    appendLog(QString("> %1").arg(command));
    appendLog(QString::fromStdString(dispatcher_.execute(command.toStdString())));
    commandLine_->clear();
    canvas_->setProject(&controller_.project());
    refreshLayers();
}

void MainWindow::completeConsoleCommand() {
    const auto text = commandLine_->text();
    if (text.startsWith('!')) return;
    const auto prefix = text.section(' ', 0, 0);
    const auto matches = dispatcher_.completions(prefix.toStdString());
    if (matches.empty()) return;
    if (matches.size() == 1) {
        const QString newText = QString::fromStdString(matches.front());
        if (!text.contains(' ')) commandLine_->setText(newText + ' ');
    } else {
        QStringList lines;
        for (const auto& match : matches) lines << QString::fromStdString(match);
        appendLog(QString("Suggestions: %1").arg(lines.join(", ")));
    }
}

void MainWindow::onLayerItemChanged(QTreeWidgetItem* item, int) {
    if (item == nullptr) return;
    const bool visible = item->checkState(0) == Qt::Checked;
    if (auto* layer = controller_.project().findLayer(item->data(0, Qt::UserRole).toString().toStdString())) {
        layer->visible = visible;
    }
    canvas_->update();
}

void MainWindow::onCurrentLayerChanged(QTreeWidgetItem* current, QTreeWidgetItem*) {
    if (current == nullptr) {
        canvas_->setSelectedLayerId(std::string{});
        return;
    }
    canvas_->setSelectedLayerId(current->data(0, Qt::UserRole).toString().toStdString());
}

void MainWindow::onLayerItemClicked(QTreeWidgetItem* item, int) {
    if (item == nullptr) return;
    canvas_->setSelectedLayerId(item->data(0, Qt::UserRole).toString().toStdString());
}

void MainWindow::onPointSelectionChanged(const QString& layerId, int selectedCount, int totalCount, bool wholeLayer) {
    if (layerId.isEmpty()) {
        statusBar()->clearMessage();
        return;
    }

    QString layerLabel = layerId;
    if (const auto* layer = controller_.project().findLayer(layerId.toStdString())) {
        layerLabel = QString::fromStdString(layer->name);
    }

    const QString message = wholeLayer
        ? QString("Selected entire layer '%1' (%2 point(s)). Shift+drag on the plot to restrict plugin input.").arg(layerLabel).arg(totalCount)
        : QString("Selected %1 of %2 point(s) in layer '%3'.").arg(selectedCount).arg(totalCount).arg(layerLabel);
    statusBar()->showMessage(message, 5000);
}

void MainWindow::onLayerDoubleClicked(QTreeWidgetItem*, int) {
    editSelectedLayer();
}

void MainWindow::applyTheme(const QString& theme) {
    controller_.project().settings().uiTheme = theme.toStdString();
    if (theme == "dark") {
        qApp->setStyleSheet(
            "QWidget { background: #202124; color: #e8eaed; }"
            "QLineEdit, QTextEdit, QTreeWidget, QTableWidget, QComboBox, QSpinBox, QDoubleSpinBox { background: #2b2c30; color: #e8eaed; border: 1px solid #5f6368; padding: 4px; }"
            "QMenuBar::item:selected, QMenu::item:selected { background: #3c4043; }"
            "QPushButton { background: #3c4043; border: 1px solid #5f6368; padding: 6px; }"
        );
    } else {
        qApp->setStyleSheet(QString());
    }
    canvas_->update();
}

void MainWindow::applyUiScale(int percent) {
    controller_.project().settings().uiFontPercent = percent;
    QFont scaled = baseFont_;
    if (baseFont_.pointSizeF() > 0) scaled.setPointSizeF(baseFont_.pointSizeF() * static_cast<double>(percent) / 100.0);
    qApp->setFont(scaled);
    refreshLayers();
}

void MainWindow::applyThemeLight() { applyTheme("light"); }
void MainWindow::applyThemeDark() { applyTheme("dark"); }
void MainWindow::setUiScale100() { applyUiScale(100); }
void MainWindow::setUiScale115() { applyUiScale(115); }
void MainWindow::setUiScale130() { applyUiScale(130); }
void MainWindow::resetView() { canvas_->resetViewToProject(); }

} // namespace plotapp::ui
