#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;

namespace plotapp::ui {

class TextEntryDialog : public QDialog {
    Q_OBJECT
public:
    explicit TextEntryDialog(const QString& title, const QString& label, const QString& value, QWidget* parent = nullptr);

    QString text() const;

private slots:
    void updateHint();

private:
    QLabel* label_ {nullptr};
    QLineEdit* edit_ {nullptr};
    QLabel* hint_ {nullptr};
};

} // namespace plotapp::ui
