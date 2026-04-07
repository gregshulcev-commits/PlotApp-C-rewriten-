#pragma once

#include <QDialog>
#include <QProcess>
#include <QStringList>

class QCloseEvent;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;

namespace plotapp::ui {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const QString& theme, int scalePercent, QWidget* parent = nullptr);

    QString theme() const;
    int scalePercent() const;

protected:
    void accept() override;
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private slots:
    void checkForUpdates();
    void installUpdates();
    void onUpdateProcessOutput();
    void onUpdateProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    enum class UpdateAction {
        None,
        Check,
        Install
    };

    void buildGeneralTab(QTabWidget* tabs);
    void buildUpdateTab(QTabWidget* tabs);
    void refreshManagedInstallInfo();
    void refreshUpdateUiState();
    void appendUpdateLog(const QString& text);
    void startUpdateAction(UpdateAction action, const QStringList& arguments);
    void setBusyState(bool busy, const QString& statusText = {});
    bool isBusy() const;
    QString valueOrUnavailable(const QString& value) const;
    QString formatInstalledAt(const QString& isoUtc) const;

    QComboBox* themeBox_ {nullptr};
    QSpinBox* scaleBox_ {nullptr};
    QLabel* buildVersionValue_ {nullptr};
    QLabel* installationModeValue_ {nullptr};
    QLabel* installedVersionValue_ {nullptr};
    QLabel* installedAtValue_ {nullptr};
    QLabel* installedCommitValue_ {nullptr};
    QLabel* repoUrlValue_ {nullptr};
    QLabel* branchValue_ {nullptr};
    QLabel* remoteCommitValue_ {nullptr};
    QLabel* statusValue_ {nullptr};
    QPushButton* checkUpdatesButton_ {nullptr};
    QPushButton* installUpdatesButton_ {nullptr};
    QPlainTextEdit* updateLog_ {nullptr};
    QDialogButtonBox* buttons_ {nullptr};
    QProcess* updateProcess_ {nullptr};
    UpdateAction updateAction_ {UpdateAction::None};
    QString combinedProcessOutput_;
    QString currentManifestPath_;
    QString currentInstallHome_;
    QString currentSystemManagerPath_;
    QString currentRepoUrl_;
};

} // namespace plotapp::ui
