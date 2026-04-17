#include "SettingsDialog.h"

#include "DialogUtil.h"

#include "plotapp/BuildInfo.h"
#include "plotapp/ManagedInstall.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextCursor>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>

namespace plotapp::ui {
namespace {

QLabel* createValueLabel(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

bool pathStartsWith(const std::filesystem::path& value, const std::filesystem::path& prefix) {
    if (prefix.empty()) return false;
    auto valueIt = value.begin();
    auto prefixIt = prefix.begin();
    for (; prefixIt != prefix.end(); ++prefixIt, ++valueIt) {
        if (valueIt == value.end() || *valueIt != *prefixIt) return false;
    }
    return true;
}

} // namespace

SettingsDialog::SettingsDialog(const QString& theme, int scalePercent, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Settings");
    applyDialogWindowSize(this, QSize(860, 620), QSize(720, 500));

    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    buildGeneralTab(tabs);
    buildUpdateTab(tabs);
    layout->addWidget(tabs);

    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons_, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    layout->addWidget(buttons_);

    const int themeIndex = std::max(0, themeBox_->findData(theme));
    themeBox_->setCurrentIndex(themeIndex);
    scaleBox_->setValue(scalePercent);

    buildVersionValue_->setText(valueOrUnavailable(QString::fromStdString(plotapp::plotappVersionString())));
    remoteCommitValue_->setText("Not checked yet");
    statusValue_->setText("Detecting installation metadata...");
    refreshManagedInstallInfo();
}

void SettingsDialog::buildGeneralTab(QTabWidget* tabs) {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    auto* form = new QFormLayout();

    themeBox_ = new QComboBox(page);
    themeBox_->addItem("Dark", "dark");
    themeBox_->addItem("Light", "light");

    scaleBox_ = new QSpinBox(page);
    scaleBox_->setRange(80, 200);
    scaleBox_->setSingleStep(5);

    form->addRow("Theme", themeBox_);
    form->addRow("UI scale (%)", scaleBox_);
    layout->addLayout(form);
    layout->addStretch(1);
    tabs->addTab(page, "General");
}

void SettingsDialog::buildUpdateTab(QTabWidget* tabs) {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* infoLabel = new QLabel(
        "This tab reuses the managed install/update workflow from install_app.sh and update_app.sh. "
        "It is available when PlotApp is launched from a managed installation.",
        page);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    auto* form = new QFormLayout();
    buildVersionValue_ = createValueLabel(page);
    installationModeValue_ = createValueLabel(page);
    installedVersionValue_ = createValueLabel(page);
    installedAtValue_ = createValueLabel(page);
    installedCommitValue_ = createValueLabel(page);
    repoUrlValue_ = createValueLabel(page);
    branchValue_ = createValueLabel(page);
    remoteCommitValue_ = createValueLabel(page);
    statusValue_ = createValueLabel(page);

    form->addRow("Build version", buildVersionValue_);
    form->addRow("Install mode", installationModeValue_);
    form->addRow("Installed version", installedVersionValue_);
    form->addRow("Installed at", installedAtValue_);
    form->addRow("Installed commit", installedCommitValue_);
    form->addRow("Repository", repoUrlValue_);
    form->addRow("Branch", branchValue_);
    form->addRow("Latest remote commit", remoteCommitValue_);
    form->addRow("Status", statusValue_);
    layout->addLayout(form);

    auto* buttonRow = new QHBoxLayout();
    checkUpdatesButton_ = new QPushButton("Check updates", page);
    installUpdatesButton_ = new QPushButton("Update", page);
    connect(checkUpdatesButton_, &QPushButton::clicked, this, &SettingsDialog::checkForUpdates);
    connect(installUpdatesButton_, &QPushButton::clicked, this, &SettingsDialog::installUpdates);
    buttonRow->addWidget(checkUpdatesButton_);
    buttonRow->addWidget(installUpdatesButton_);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    updateLog_ = new QPlainTextEdit(page);
    updateLog_->setReadOnly(true);
    updateLog_->setMinimumHeight(200);
    updateLog_->setPlaceholderText("Updater output will appear here.");
    layout->addWidget(updateLog_, 1);

    tabs->addTab(page, "Updates");
}

QString SettingsDialog::theme() const { return themeBox_->currentData().toString(); }
int SettingsDialog::scalePercent() const { return scaleBox_->value(); }

void SettingsDialog::refreshManagedInstallInfo() {
    currentManifestPath_.clear();
    currentInstallHome_.clear();
    currentSystemManagerPath_.clear();
    currentRepoUrl_.clear();

    const QString buildVersion = valueOrUnavailable(QString::fromStdString(plotapp::plotappVersionString()));
    buildVersionValue_->setText(buildVersion);
    buildVersionValue_->setToolTip(buildVersion);
    remoteCommitValue_->setText("Not checked yet");
    remoteCommitValue_->setToolTip({});

    const auto executablePath = std::filesystem::path(QCoreApplication::applicationFilePath().toStdString());
    const auto manifestPath = plotapp::findManagedInstallManifest(plotapp::managedInstallManifestCandidates(executablePath));
    if (!manifestPath.has_value()) {
        installationModeValue_->setText("Development / unmanaged run");
        installedVersionValue_->setText(buildVersion);
        installedAtValue_->setText("Not available");
        installedAtValue_->setToolTip({});
        installedCommitValue_->setText("Not available");
        installedCommitValue_->setToolTip({});
        repoUrlValue_->setText("Not configured");
        repoUrlValue_->setToolTip({});
        branchValue_->setText("Not configured");
        branchValue_->setToolTip({});
        statusValue_->setText("Managed install metadata was not found. GUI update actions are unavailable in this run mode.");
        statusValue_->setToolTip({});
        refreshUpdateUiState();
        return;
    }

    const auto info = plotapp::loadManagedInstallInfo(*manifestPath);
    if (!info.has_value() || !info->valid) {
        installationModeValue_->setText("Managed install (metadata unavailable)");
        installedVersionValue_->setText(buildVersion);
        installedAtValue_->setText("Not available");
        installedAtValue_->setToolTip(QString::fromStdString(manifestPath->string()));
        installedCommitValue_->setText("Not available");
        installedCommitValue_->setToolTip({});
        repoUrlValue_->setText("Not configured");
        repoUrlValue_->setToolTip({});
        branchValue_->setText("Not configured");
        branchValue_->setToolTip({});
        statusValue_->setText("Managed installation metadata could not be read.");
        statusValue_->setToolTip(QString::fromStdString(manifestPath->string()));
        refreshUpdateUiState();
        return;
    }

    const auto normalizedExecutable = executablePath.lexically_normal();
    const auto normalizedInstallHome = std::filesystem::path(info->installHome).lexically_normal();
    if (!info->installHome.empty() && !pathStartsWith(normalizedExecutable, normalizedInstallHome)) {
        installationModeValue_->setText("Development / unmanaged run");
        installedVersionValue_->setText(buildVersion);
        installedAtValue_->setText("Not available");
        installedAtValue_->setToolTip({});
        installedCommitValue_->setText("Not available");
        installedCommitValue_->setToolTip({});
        repoUrlValue_->setText("Not configured");
        repoUrlValue_->setToolTip({});
        branchValue_->setText("Not configured");
        branchValue_->setToolTip({});
        statusValue_->setText("A managed installation exists on this system, but the current binary is not running from that managed payload. GUI update actions stay disabled in this run mode.");
        statusValue_->setToolTip(QString::fromStdString(info->manifestPath.string()));
        refreshUpdateUiState();
        return;
    }

    currentManifestPath_ = QString::fromStdString(info->manifestPath.string());
    currentInstallHome_ = QString::fromStdString(info->installHome);
    currentSystemManagerPath_ = QString::fromStdString(info->systemManagerPath);
    currentRepoUrl_ = QString::fromStdString(info->repoUrl);

    installationModeValue_->setText("Managed install");
    installationModeValue_->setToolTip(currentManifestPath_);

    const QString installedVersion = valueOrUnavailable(QString::fromStdString(info->installedVersion));
    installedVersionValue_->setText(installedVersion);
    installedVersionValue_->setToolTip(installedVersion);

    const QString installedAtRaw = QString::fromStdString(info->installedAt);
    installedAtValue_->setText(formatInstalledAt(installedAtRaw));
    installedAtValue_->setToolTip(installedAtRaw);

    const QString fullCommit = QString::fromStdString(info->sourceCommit);
    const QString shortCommit = QString::fromStdString(info->sourceCommitShort);
    const QString commitDisplay = valueOrUnavailable(shortCommit.isEmpty() ? fullCommit : shortCommit);
    installedCommitValue_->setText(commitDisplay);
    installedCommitValue_->setToolTip(fullCommit);

    const QString repoUrl = valueOrUnavailable(QString::fromStdString(info->repoUrl));
    repoUrlValue_->setText(repoUrl);
    repoUrlValue_->setToolTip(QString::fromStdString(info->repoUrl));

    const QString branch = valueOrUnavailable(QString::fromStdString(info->branch));
    branchValue_->setText(branch);
    branchValue_->setToolTip(QString::fromStdString(info->branch));

    QFileInfo managerInfo(currentSystemManagerPath_);
    if (currentSystemManagerPath_.isEmpty()) {
        statusValue_->setText("Managed install detected, but the system manager path is missing from the manifest.");
    } else if (!managerInfo.exists()) {
        statusValue_->setText("Managed install detected, but the update manager script was not found on disk.");
    } else if (!managerInfo.isExecutable()) {
        statusValue_->setText("Managed install detected, but the update manager script is not executable.");
    } else if (currentRepoUrl_.trimmed().isEmpty()) {
        statusValue_->setText("Repository is not configured for updates. Bind a Git repository first with update_app.sh --set-repo ...");
    } else {
        statusValue_->setText("Ready to check for updates.");
    }
    statusValue_->setToolTip(currentManifestPath_);

    refreshUpdateUiState();
}

void SettingsDialog::refreshUpdateUiState() {
    const QFileInfo managerInfo(currentSystemManagerPath_);
    const bool managedReady = !currentInstallHome_.trimmed().isEmpty() && managerInfo.exists() && managerInfo.isFile() && managerInfo.isExecutable();
    const bool repoConfigured = !currentRepoUrl_.trimmed().isEmpty();
    const bool canRun = managedReady && repoConfigured && !isBusy();

    if (checkUpdatesButton_ != nullptr) checkUpdatesButton_->setEnabled(canRun);
    if (installUpdatesButton_ != nullptr) installUpdatesButton_->setEnabled(canRun);
    if (buttons_ != nullptr) buttons_->setEnabled(!isBusy());
    if (themeBox_ != nullptr) themeBox_->setEnabled(!isBusy());
    if (scaleBox_ != nullptr) scaleBox_->setEnabled(!isBusy());
}

void SettingsDialog::appendUpdateLog(const QString& text) {
    if (updateLog_ == nullptr || text.isEmpty()) return;
    updateLog_->moveCursor(QTextCursor::End);
    updateLog_->insertPlainText(text);
    if (!text.endsWith('\n')) updateLog_->insertPlainText("\n");
    updateLog_->moveCursor(QTextCursor::End);
}

void SettingsDialog::setBusyState(bool busy, const QString& statusText) {
    if (!statusText.trimmed().isEmpty() && statusValue_ != nullptr) {
        statusValue_->setText(statusText);
    }
    Q_UNUSED(busy);
    refreshUpdateUiState();
}

bool SettingsDialog::isBusy() const {
    return updateProcess_ != nullptr && updateProcess_->state() != QProcess::NotRunning;
}

QString SettingsDialog::valueOrUnavailable(const QString& value) const {
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("Not available") : trimmed;
}

QString SettingsDialog::formatInstalledAt(const QString& isoUtc) const {
    const QString trimmed = isoUtc.trimmed();
    if (trimmed.isEmpty()) return QStringLiteral("Not available");
    const QDateTime parsed = QDateTime::fromString(trimmed, Qt::ISODate);
    if (!parsed.isValid()) return trimmed;
    return parsed.toLocalTime().toString("yyyy-MM-dd HH:mm:ss t");
}

void SettingsDialog::startUpdateAction(UpdateAction action, const QStringList& arguments) {
    if (isBusy()) return;

    QFileInfo managerInfo(currentSystemManagerPath_);
    if (currentInstallHome_.trimmed().isEmpty() || !managerInfo.exists() || !managerInfo.isFile() || !managerInfo.isExecutable()) {
        QMessageBox::warning(this, "Updates", "The managed-install update manager is not available in this run mode.");
        return;
    }
    if (currentRepoUrl_.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Updates", "A Git repository is not configured for updates. Use update_app.sh --set-repo first.");
        return;
    }

    combinedProcessOutput_.clear();
    updateAction_ = action;
    updateProcess_ = new QProcess(this);
    connect(updateProcess_, &QProcess::readyReadStandardOutput, this, &SettingsDialog::onUpdateProcessOutput);
    connect(updateProcess_, &QProcess::readyReadStandardError, this, &SettingsDialog::onUpdateProcessOutput);
    connect(updateProcess_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &SettingsDialog::onUpdateProcessFinished);

    QStringList fullArguments;
    fullArguments << "update" << "--install-home" << currentInstallHome_;
    fullArguments.append(arguments);

    updateProcess_->setProgram(currentSystemManagerPath_);
    updateProcess_->setArguments(fullArguments);
    appendUpdateLog(QStringLiteral("$ %1 %2").arg(currentSystemManagerPath_, fullArguments.join(' ')));
    setBusyState(true, action == UpdateAction::Check ? QStringLiteral("Checking Git repository...")
                                                     : QStringLiteral("Installing update from the configured repository..."));
    updateProcess_->start();
    if (!updateProcess_->waitForStarted(3000)) {
        const QString errorText = updateProcess_->errorString();
        appendUpdateLog(QStringLiteral("Failed to start updater: %1").arg(errorText));
        statusValue_->setText(QStringLiteral("Failed to start the updater."));
        updateProcess_->deleteLater();
        updateProcess_ = nullptr;
        updateAction_ = UpdateAction::None;
        refreshUpdateUiState();
        QMessageBox::warning(this, "Updates", QStringLiteral("Failed to start the updater.\n\n%1").arg(errorText));
    }
}

void SettingsDialog::checkForUpdates() {
    startUpdateAction(UpdateAction::Check, {"--check-only"});
}

void SettingsDialog::installUpdates() {
    const auto answer = QMessageBox::question(
        this,
        "Install update",
        "PlotApp will fetch the configured Git repository, rebuild a fresh managed payload, and replace the current installation.\n\nContinue?");
    if (answer != QMessageBox::Yes) return;
    startUpdateAction(UpdateAction::Install, {"--yes"});
}

void SettingsDialog::onUpdateProcessOutput() {
    if (updateProcess_ == nullptr) return;

    const QString stdoutText = QString::fromLocal8Bit(updateProcess_->readAllStandardOutput());
    if (!stdoutText.isEmpty()) {
        combinedProcessOutput_ += stdoutText;
        appendUpdateLog(stdoutText);
    }

    const QString stderrText = QString::fromLocal8Bit(updateProcess_->readAllStandardError());
    if (!stderrText.isEmpty()) {
        combinedProcessOutput_ += stderrText;
        appendUpdateLog(stderrText);
    }
}

void SettingsDialog::onUpdateProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    onUpdateProcessOutput();

    const UpdateAction finishedAction = updateAction_;
    const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;
    const auto parsedStatus = plotapp::parseManagedInstallUpdateStatus(combinedProcessOutput_.toStdString());

    if (updateProcess_ != nullptr) {
        updateProcess_->deleteLater();
        updateProcess_ = nullptr;
    }
    updateAction_ = UpdateAction::None;

    if (finishedAction == UpdateAction::Check) {
        if (success) {
            if (parsedStatus.valid) {
                if (!parsedStatus.installedVersion.empty()) {
                    const QString value = valueOrUnavailable(QString::fromStdString(parsedStatus.installedVersion));
                    installedVersionValue_->setText(value);
                    installedVersionValue_->setToolTip(value);
                }
                if (!parsedStatus.installedCommit.empty()) {
                    const QString commit = valueOrUnavailable(QString::fromStdString(parsedStatus.installedCommit));
                    installedCommitValue_->setText(commit);
                    installedCommitValue_->setToolTip(QString::fromStdString(parsedStatus.installedCommit));
                }
                if (!parsedStatus.remoteCommit.empty()) {
                    const QString remoteCommit = valueOrUnavailable(QString::fromStdString(parsedStatus.remoteCommit));
                    remoteCommitValue_->setText(remoteCommit);
                    remoteCommitValue_->setToolTip(QString::fromStdString(parsedStatus.remoteCommit));
                }
                if (!parsedStatus.status.empty()) {
                    statusValue_->setText(QString::fromStdString(parsedStatus.status));
                } else {
                    statusValue_->setText("Update check completed.");
                }
            } else {
                statusValue_->setText("Update check completed.");
            }
        } else {
            statusValue_->setText("Update check failed. See the updater log for details.");
            QMessageBox::warning(this, "Check updates", "Failed to check for updates.\n\nSee the updater log for details.");
        }
    } else if (finishedAction == UpdateAction::Install) {
        refreshManagedInstallInfo();
        if (success) {
            const QString status = QString::fromStdString(parsedStatus.status);
            if (status.compare("already up to date", Qt::CaseInsensitive) == 0 ||
                status.compare("up to date", Qt::CaseInsensitive) == 0) {
                if (!parsedStatus.remoteCommit.empty()) {
                    const QString remoteCommit = valueOrUnavailable(QString::fromStdString(parsedStatus.remoteCommit));
                    remoteCommitValue_->setText(remoteCommit);
                    remoteCommitValue_->setToolTip(QString::fromStdString(parsedStatus.remoteCommit));
                }
                statusValue_->setText("Already up to date.");
                QMessageBox::information(this, "Update", "The installed revision is already up to date.");
            } else {
                if (!parsedStatus.remoteCommit.empty()) {
                    const QString remoteCommit = valueOrUnavailable(QString::fromStdString(parsedStatus.remoteCommit));
                    remoteCommitValue_->setText(remoteCommit);
                    remoteCommitValue_->setToolTip(QString::fromStdString(parsedStatus.remoteCommit));
                }
                statusValue_->setText("Update installed. Restart PlotApp to launch the new build.");
                appendUpdateLog("Update installed successfully. Restart PlotApp to launch the new build.");
                QMessageBox::information(this, "Update", "Update installed successfully.\n\nRestart PlotApp to launch the new build.");
            }
        } else {
            statusValue_->setText("Update failed. See the updater log for details.");
            QMessageBox::warning(this, "Update", "Failed to install the update.\n\nSee the updater log for details.");
        }
    }

    refreshUpdateUiState();
}

void SettingsDialog::accept() {
    if (isBusy()) {
        QMessageBox::warning(this, "Updates", "Wait for the current update operation to finish before closing Settings.");
        return;
    }
    QDialog::accept();
}

void SettingsDialog::closeEvent(QCloseEvent* event) {
    if (isBusy()) {
        QMessageBox::warning(this, "Updates", "Wait for the current update operation to finish before closing Settings.");
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void SettingsDialog::reject() {
    if (isBusy()) {
        QMessageBox::warning(this, "Updates", "Wait for the current update operation to finish before closing Settings.");
        return;
    }
    QDialog::reject();
}

} // namespace plotapp::ui
