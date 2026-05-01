#!/usr/bin/env bash
set -eu
root=${1:-$(pwd)}
cd "$root"
check() {
  pattern=$1
  file=$2
  message=$3
  if ! grep -q -- "$pattern" "$file"; then
    echo "FAIL: $message" >&2
    exit 1
  fi
}
check 'DPI is saved as metadata after drawing' src/ui/PlotCanvasWidget.cpp 'export must not paint with high-DPI font scaling'
check 'logicalImage.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)' src/ui/PlotCanvasWidget.cpp 'export must scale the visible canvas snapshot'
check 'legendClicked(const QString& layerId)' src/ui/PlotCanvasWidget.h 'legend click signal missing'
check 'connect(canvas_, &PlotCanvasWidget::legendClicked' src/ui/MainWindow.cpp 'legend click must open inline editor'
check 'QPlainTextEdit' src/ui/LayerPropertiesDialog.cpp 'layer legend editor must be multiline'
check 'adjustLegendEditorHeight' src/ui/LayerPropertiesDialog.cpp 'layer legend editor must auto-resize by line count'
check 'autosave.plotapp' src/ui/MainWindow.cpp 'autosave cache file path missing'
check 'restoreAutosaveIfPresent' src/ui/MainWindow.cpp 'autosave restore logic missing'
check 'New project in new window' src/ui/MainWindow.cpp 'new project should open a new window'
check 'Fit canvas to A4 landscape' src/ui/MainWindow.cpp 'A4 landscape canvas action missing'
check 'fillRect(activeRenderRect(), bg)' src/ui/PlotCanvasWidget.cpp 'canvas paint path missing'
check 'const QColor fg = dark ? QColor("#ffffff")' src/ui/PlotCanvasWidget.cpp 'dark theme foreground must be white'
echo 'GUI static checks passed.'
