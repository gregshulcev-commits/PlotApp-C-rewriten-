#include "TextEntryDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace plotapp::ui {
namespace {
QString latexHintText(const QString& text) {
    if (!text.contains('\\')) return "Supported shortcuts: _{i}, ^{2}, \\mu, \\sigma, \\Delta, \\alpha, \\beta, \\lambda, \\pi";
    return "Examples: T_{room}, x^2, \\mu_A, \\sigma, \\Delta t, \\alpha_{max}";
}
}

TextEntryDialog::TextEntryDialog(const QString& title, const QString& label, const QString& value, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);
    auto* layout = new QVBoxLayout(this);
    label_ = new QLabel(label, this);
    edit_ = new QLineEdit(value, this);
    hint_ = new QLabel(this);
    hint_->setWordWrap(true);
    layout->addWidget(label_);
    layout->addWidget(edit_);
    layout->addWidget(hint_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(edit_, &QLineEdit::textChanged, this, &TextEntryDialog::updateHint);
    layout->addWidget(buttons);
    updateHint();
    edit_->selectAll();
    edit_->setFocus();
}

QString TextEntryDialog::text() const { return edit_->text(); }

void TextEntryDialog::updateHint() {
    hint_->setText(latexHintText(edit_->text()));
}

} // namespace plotapp::ui
