#pragma once

#include <cmath>

#include <QDialog>
#include <QSize>

namespace plotapp::ui {

inline void applyDialogWindowSize(QDialog* dialog,
                                  const QSize& preferred = QSize(820, 520),
                                  const QSize& minimum = QSize(680, 420)) {
    if (dialog == nullptr) return;
    dialog->resize(preferred);
    dialog->setMinimumSize(minimum);
}

inline QSize a4PixelsAtDpi(int dpi, bool landscape = false) {
    const int safeDpi = dpi > 0 ? dpi : 150;
    const int portraitWidth = static_cast<int>(std::lround((210.0 / 25.4) * safeDpi));
    const int portraitHeight = static_cast<int>(std::lround((297.0 / 25.4) * safeDpi));
    return landscape ? QSize(portraitHeight, portraitWidth) : QSize(portraitWidth, portraitHeight);
}

} // namespace plotapp::ui
