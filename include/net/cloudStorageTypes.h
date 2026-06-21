/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef CLOUDSTORAGETYPES_H
#define CLOUDSTORAGETYPES_H

#include <QString>
#include <QDateTime>
#include <QMetaType>
#include <QVector>

/**
 * Shared types for the cloud storage subsystem.
 *
 * See specs/005-cloud-storage/data-model.md. FileLu and AWS S3 are both
 * S3-compatible and differ only by S3Config (endpoint/region/bucket/keys).
 */

enum class CloudProviderType {
    FileLu,        // functional — S3-compatible "S5" object storage (s5lu.com)
    AwsS3,         // functional — Amazon S3
    Dropbox,       // stub — "Not implemented yet"
    GoogleDrive,   // stub — "Not implemented yet"
    ICloudDrive    // stub — "Not implemented yet"
};

namespace CloudProvider {

inline bool isFunctional(CloudProviderType t) {
    return t == CloudProviderType::FileLu || t == CloudProviderType::AwsS3;
}

inline QString displayName(CloudProviderType t) {
    switch (t) {
    case CloudProviderType::FileLu:      return QStringLiteral("FileLu");
    case CloudProviderType::AwsS3:       return QStringLiteral("AWS S3");
    case CloudProviderType::Dropbox:     return QStringLiteral("Dropbox");
    case CloudProviderType::GoogleDrive: return QStringLiteral("Google Drive");
    case CloudProviderType::ICloudDrive: return QStringLiteral("iCloud Drive");
    }
    return QString();
}

inline QString signupUrl(CloudProviderType t) {
    switch (t) {
    case CloudProviderType::FileLu:      return QStringLiteral("https://filelu.com");
    case CloudProviderType::AwsS3:       return QStringLiteral("https://aws.amazon.com/s3/");
    case CloudProviderType::Dropbox:     return QStringLiteral("https://www.dropbox.com");
    case CloudProviderType::GoogleDrive: return QStringLiteral("https://drive.google.com");
    case CloudProviderType::ICloudDrive: return QStringLiteral("https://www.icloud.com");
    }
    return QString();
}

// Short tag used to prefix debug-log lines, e.g. "[filelu]".
inline QString logTag(CloudProviderType t) {
    switch (t) {
    case CloudProviderType::FileLu:      return QStringLiteral("filelu");
    case CloudProviderType::AwsS3:       return QStringLiteral("aws");
    case CloudProviderType::Dropbox:     return QStringLiteral("dropbox");
    case CloudProviderType::GoogleDrive: return QStringLiteral("gdrive");
    case CloudProviderType::ICloudDrive: return QStringLiteral("icloud");
    }
    return QStringLiteral("cloud");
}

// Settings key used to persist a functional provider's config.
inline QString settingsKey(CloudProviderType t) {
    switch (t) {
    case CloudProviderType::FileLu: return QStringLiteral("filelu");
    case CloudProviderType::AwsS3:  return QStringLiteral("awss3");
    default:                        return QString();
    }
}

} // namespace CloudProvider

/**
 * S3 connection parameters. Drives the signer and the worker.
 * pathStyle is true for FileLu and the default everywhere (path-style addressing:
 * https://<endpoint-host>/<bucket>/<key>).
 */
struct S3Config {
    QString endpoint;   // e.g. "https://s5lu.com" or "https://s3.us-east-1.amazonaws.com"
    QString region;     // e.g. "global" (FileLu) or "us-east-1"
    QString bucket;
    QString accessKey;
    QString secretKey;
    bool    pathStyle = true;

    bool isComplete() const {
        return !endpoint.isEmpty() && !region.isEmpty() && !bucket.isEmpty()
            && !accessKey.isEmpty() && !secretKey.isEmpty();
    }
};

/** Persisted per-provider configuration. */
struct CloudProviderConfig {
    CloudProviderType type = CloudProviderType::FileLu;
    bool     enabled = false;
    S3Config s3;

    // Usable in the open/save chooser only when enabled and fully configured.
    bool isUsable() const { return enabled && s3.isComplete(); }
};

/** One remote .clx log as returned by ListObjectsV2. */
struct CloudObject {
    QString   key;
    qint64    size = 0;
    QDateTime lastModified;
};

/** Tracks where a cloud-opened log came from, so re-save defaults to the same place. */
struct CloudOrigin {
    bool              isCloud = false;
    CloudProviderType providerType = CloudProviderType::FileLu;
    QString           key;
};

/** High-level operation identifier used in failure reporting. */
enum class CloudOp { List, Download, Upload, Delete, Test, Exists };

Q_DECLARE_METATYPE(CloudObject)
Q_DECLARE_METATYPE(QVector<CloudObject>)
Q_DECLARE_METATYPE(CloudOp)

#endif // CLOUDSTORAGETYPES_H
