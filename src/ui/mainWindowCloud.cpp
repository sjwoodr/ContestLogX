/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * Cloud storage integration for MainWindow (specs/005-cloud-storage).
 *
 * Model: the local file is always the primary store. Cloud storage is a
 * background BACKUP - every successful local save mirrors the file to the
 * configured provider(s). Opening from the cloud downloads to a user-chosen
 * local location and then opens that local file. All network I/O runs on the
 * provider's worker thread; this file only orchestrates UI and wiring.
 */

#include "mainWindow.h"
#include "settings.h"
#include "debugLogger.h"
#include "net/cloudStorageProvider.h"
#include "net/s3StorageProvider.h"

#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QListWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QApplication>

CloudStorageProvider* MainWindow::makeCloudProvider(CloudProviderType type)
{
    CloudProviderConfig cfg = Settings::instance().getCloudProviderConfig(type);
    if (!cfg.isUsable())
        return nullptr;
    return new S3StorageProvider(type, cfg.s3, this);
}

bool MainWindow::chooseOpenSource(const QVector<CloudProviderConfig>& providers,
                                  bool& useLocalOut, CloudProviderType& chosenOut)
{
    QDialog dlg(this);
    dlg.setWindowTitle("Open Log From");
    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel("Choose where to open a log from:"));

    QListWidget* list = new QListWidget(&dlg);
    // Local is always first and pre-selected (keyboard-first: Enter = Local).
    QListWidgetItem* localItem = new QListWidgetItem("Local filesystem", list);
    localItem->setData(Qt::UserRole, -1);
    for (const CloudProviderConfig& p : providers) {
        QListWidgetItem* item =
            new QListWidgetItem(CloudProvider::displayName(p.type) + "  (cloud backup)", list);
        item->setData(Qt::UserRole, static_cast<int>(p.type));
    }
    list->setCurrentRow(0);
    layout->addWidget(list);

    QDialogButtonBox* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    const int role = list->currentItem()->data(Qt::UserRole).toInt();
    if (role < 0) {
        useLocalOut = true;
        DebugLogger::instance().log("CloudStorage", "Open chooser: local filesystem");
    } else {
        useLocalOut = false;
        chosenOut = static_cast<CloudProviderType>(role);
        DebugLogger::instance().log("CloudStorage",
            QString("[%1] Open chooser: cloud selected").arg(CloudProvider::logTag(chosenOut)));
    }
    return true;
}

void MainWindow::openLogFromCloud(CloudProviderType type)
{
    DebugLogger::instance().log("CloudStorage",
        QString("[%1] Open from cloud: listing bucket").arg(CloudProvider::logTag(type)));
    CloudStorageProvider* provider = makeCloudProvider(type);
    if (!provider) {
        QMessageBox::warning(this, "Cloud Storage",
            CloudProvider::displayName(type) + " is not fully configured.");
        return;
    }

    auto* progress = new QProgressDialog("Listing cloud logs...", "Cancel", 0, 0, this);
    progress->setWindowTitle("Cloud Storage");
    progress->setWindowModality(Qt::WindowModal);
    progress->show();
    QApplication::processEvents();

    connect(provider, &CloudStorageProvider::operationFailed, this,
            [this, provider, progress](CloudOp, const QString& message) {
        progress->close();
        progress->deleteLater();
        QMessageBox::warning(this, "Cloud Storage", message);
        provider->deleteLater();
    }, Qt::SingleShotConnection);

    connect(provider, &CloudStorageProvider::listReady, this,
            [this, provider, progress, type](const QVector<CloudObject>& objects) {
        progress->close();
        progress->deleteLater();

        if (objects.isEmpty()) {
            QMessageBox::information(this, "Cloud Storage",
                "No .clx logs found in this bucket.");
            provider->deleteLater();
            return;
        }

        // Pick the cloud object to restore.
        QDialog dlg(this);
        dlg.setWindowTitle("Open Log from " + CloudProvider::displayName(type));
        dlg.setMinimumWidth(520);   // wide enough for full name + size + timestamp
        QVBoxLayout* layout = new QVBoxLayout(&dlg);
        layout->addWidget(new QLabel("Select a log to download and open:"));
        QListWidget* list = new QListWidget(&dlg);
        for (const CloudObject& obj : objects) {
            QString label = QStringLiteral("%1   (%2 KB, %3)")
                .arg(obj.key)
                .arg((obj.size + 1023) / 1024)
                .arg(obj.lastModified.toString("yyyy-MM-dd hh:mm"));
            QListWidgetItem* item = new QListWidgetItem(label, list);
            item->setData(Qt::UserRole, obj.key);
        }
        list->setCurrentRow(0);
        layout->addWidget(list);
        QDialogButtonBox* buttons =
            new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, &dlg);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);

        if (dlg.exec() != QDialog::Accepted || !list->currentItem()) {
            provider->deleteLater();
            return;
        }
        const QString key = list->currentItem()->data(Qt::UserRole).toString();

        // Explain why we're about to ask for a local location, so the operator
        // doesn't think they have to hunt for the log again.
        QMessageBox::information(this, "Choose a Local Location",
            "Choose a location to save a local copy of \"" + key + "\".\n\n"
            "Logs are stored locally for fast disk access and automatically synced "
            "to your cloud provider each time you save.");

        // Prompt where the LOCAL copy should reside (cloud is a backup, the local
        // file is the working copy from here on).
        QString localPath = QFileDialog::getSaveFileName(this,
            "Save Local Copy As", key, "ContestLogX 2.0 Format (*.clx)");
        if (localPath.isEmpty()) {
            provider->deleteLater();
            return;
        }
        if (!localPath.endsWith(".clx", Qt::CaseInsensitive))
            localPath += ".clx";

        DebugLogger::instance().log("CloudStorage",
            QString("[%1] Open from cloud: downloading '%2' to local file")
                .arg(CloudProvider::logTag(type), key));

        auto* dlProgress = new QProgressDialog("Downloading log...", QString(), 0, 0, this);
        dlProgress->setWindowTitle("Cloud Storage");
        dlProgress->setWindowModality(Qt::WindowModal);
        dlProgress->show();
        QApplication::processEvents();

        connect(provider, &CloudStorageProvider::operationFailed, this,
                [this, provider, dlProgress](CloudOp, const QString& message) {
            dlProgress->close();
            dlProgress->deleteLater();
            QMessageBox::warning(this, "Cloud Storage", message);
            provider->deleteLater();
        }, Qt::SingleShotConnection);

        connect(provider, &CloudStorageProvider::downloadReady, this,
                [this, provider, dlProgress, type, key](const QString&, const QString& path) {
            dlProgress->close();
            dlProgress->deleteLater();
            DebugLogger::instance().log("CloudStorage",
                QString("[%1] Open from cloud: downloaded '%2', opening local copy")
                    .arg(CloudProvider::logTag(type), key));
            loadLogFromPath(path);  // path becomes m_currentFile; future saves re-sync
            provider->deleteLater();
        }, Qt::SingleShotConnection);

        provider->downloadLog(key, localPath);
    }, Qt::SingleShotConnection);

    provider->listLogs();
}

void MainWindow::syncToCloudProviders(const QString& localPath)
{
    const QVector<CloudProviderConfig> providers =
        Settings::instance().getConfiguredCloudProviders();
    if (providers.isEmpty())
        return;  // no cloud backup configured - local save only

    const QString key = QFileInfo(localPath).fileName();

    for (const CloudProviderConfig& cfg : providers) {
        CloudStorageProvider* provider = makeCloudProvider(cfg.type);
        if (!provider)
            continue;
        const CloudProviderType type = cfg.type;
        const QString name = CloudProvider::displayName(type);

        DebugLogger::instance().log("CloudStorage",
            QString("[%1] Sync: backing up '%2' to cloud").arg(CloudProvider::logTag(type), key));
        m_statusLabel->setText("Syncing to " + name + "...");

        connect(provider, &CloudStorageProvider::uploadFinished, this,
                [this, provider, name, type, key](const QString&) {
            DebugLogger::instance().log("CloudStorage",
                QString("[%1] Sync: '%2' backed up OK").arg(CloudProvider::logTag(type), key));
            m_statusLabel->setText("Synced to " + name + ": " + key);
            provider->deleteLater();
        }, Qt::SingleShotConnection);

        connect(provider, &CloudStorageProvider::operationFailed, this,
                [this, provider, name, type](CloudOp, const QString& message) {
            DebugLogger::instance().log("CloudStorage",
                QString("[%1] Sync FAILED: %2").arg(CloudProvider::logTag(type), message));
            m_statusLabel->setText("Cloud sync to " + name + " failed: " + message);
            provider->deleteLater();
        }, Qt::SingleShotConnection);

        // Backup mirror: unconditional overwrite of the same-named object.
        provider->uploadLog(key, localPath);
    }
}
