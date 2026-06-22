/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "net/s3StorageWorker.h"
#include "net/s3Signer.h"
#include "debugLogger.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QFile>
#include <QUrl>
#include <QUrlQuery>
#include <QXmlStreamReader>
#include <QDateTime>
#include <QTimeZone>

static constexpr int kTransferTimeoutMs = 30000;

S3StorageWorker::S3StorageWorker(QObject* parent)
    : QObject(parent)
{
}

S3StorageWorker::~S3StorageWorker()
{
    delete m_nam;
}

void S3StorageWorker::setConfig(const S3Config& cfg)
{
    m_config = cfg;

    // The bucket field may be "bucket" or "bucket/folder/path". Split it into the
    // real bucket name (first segment) and a key prefix used for all objects.
    QString b = cfg.bucket.trimmed();
    while (b.startsWith('/')) b.remove(0, 1);
    while (b.endsWith('/'))   b.chop(1);

    const int slash = b.indexOf('/');
    if (slash >= 0) {
        m_bucketName = b.left(slash);
        m_prefix = b.mid(slash + 1);
        if (!m_prefix.isEmpty() && !m_prefix.endsWith('/'))
            m_prefix += '/';
    } else {
        m_bucketName = b;
        m_prefix.clear();
    }

    // Never log access/secret keys.
    dlog(QString("Configured S3: endpoint=%1 region=%2 bucket=%3 prefix=%4 pathStyle=%5")
            .arg(m_config.endpoint, m_config.region, m_bucketName,
                 m_prefix.isEmpty() ? QStringLiteral("(none)") : m_prefix,
                 m_config.pathStyle ? QStringLiteral("yes") : QStringLiteral("no")));
}

void S3StorageWorker::dlog(const QString& msg) const
{
    DebugLogger::instance().log("CloudStorage", QStringLiteral("[%1] %2").arg(m_tag, msg));
}

QNetworkAccessManager* S3StorageWorker::ensureNam()
{
    // Created on the worker thread on first use so all I/O stays off the GUI thread.
    if (!m_nam)
        m_nam = new QNetworkAccessManager();
    return m_nam;
}

QNetworkReply* S3StorageWorker::sendSigned(const QString& method,
                                           const QString& key,
                                           const QMap<QString, QString>& query,
                                           const QByteArray& body,
                                           const QString& contentType,
                                           QByteArray& responseBody,
                                           int& httpStatus,
                                           QString& networkError)
{
    httpStatus = 0;
    networkError.clear();
    responseBody.clear();

    const QUrl endpointUrl(m_config.endpoint);
    const QString host = endpointUrl.host();

    // Path-style: /<bucket>[/<prefix><key>]. The prefix is the configured folder
    // path within the bucket (empty if logs live at the bucket root).
    QString path = QStringLiteral("/") + m_bucketName;
    if (!key.isEmpty())
        path += QStringLiteral("/") + m_prefix + key;

    const QString payloadHash = body.isEmpty()
        ? S3Signer::emptyPayloadHash()
        : S3Signer::hashSha256Hex(body);

    S3Signer::SigningInput in;
    in.httpMethod = method;
    in.host = host;
    in.canonicalUri = path;
    in.queryParams = query;
    in.hexPayloadHash = payloadHash;
    in.region = m_config.region;
    in.accessKey = m_config.accessKey;
    in.secretKey = m_config.secretKey;
    in.utcTimestamp = QDateTime::currentDateTimeUtc();
    if (!contentType.isEmpty())
        in.headers.insert(QStringLiteral("content-type"), contentType);

    const S3Signer::SignedHeaders sig = S3Signer::sign(in);

    // Build the request URL (path-encoded the same way the signer canonicalizes it).
    QUrl url;
    url.setScheme(QStringLiteral("https"));   // HTTPS enforced (FR-019)
    url.setHost(host);
    if (endpointUrl.port() != -1)
        url.setPort(endpointUrl.port());
    url.setPath(path);
    if (!query.isEmpty()) {
        QUrlQuery q;
        for (auto it = query.constBegin(); it != query.constEnd(); ++it)
            q.addQueryItem(it.key(), it.value());
        url.setQuery(q);
    }

    QNetworkRequest req(url);
    req.setRawHeader("Host", host.toUtf8());
    req.setRawHeader("x-amz-date", sig.amzDate.toUtf8());
    req.setRawHeader("x-amz-content-sha256", sig.contentSha256.toUtf8());
    req.setRawHeader("Authorization", sig.authorization.toUtf8());
    if (!contentType.isEmpty())
        req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    req.setTransferTimeout(kTransferTimeoutMs);

    QNetworkAccessManager* nam = ensureNam();
    QNetworkReply* reply = nullptr;
    if (method == QLatin1String("GET"))
        reply = nam->get(req);
    else if (method == QLatin1String("HEAD"))
        reply = nam->head(req);
    else if (method == QLatin1String("PUT"))
        reply = nam->put(req, body);
    else if (method == QLatin1String("DELETE"))
        reply = nam->deleteResource(req);
    else
        reply = nam->sendCustomRequest(req, method.toUtf8(), body);

    // Block on the worker thread (not the GUI thread) until the reply finishes.
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError)
        networkError = reply->errorString();
    responseBody = reply->readAll();

    // Log method + path + status only - never the Authorization header or keys.
    dlog(QString("%1 %2 -> HTTP %3%4")
            .arg(method, path).arg(httpStatus)
            .arg(networkError.isEmpty() ? QString() : QStringLiteral(" [") + networkError + QLatin1Char(']')));
    return reply;
}

QString S3StorageWorker::classifyError(int httpStatus, const QString& networkError,
                                       const QByteArray& responseBody) const
{
    // Never include secret/signing material - only HTTP status and the S3 <Code>.
    if (httpStatus == 403)
        return QStringLiteral("Authentication failed - check your access/secret keys.");
    if (httpStatus == 404) {
        if (responseBody.contains("NoSuchBucket"))
            return QStringLiteral("Bucket '%1' not found.").arg(m_bucketName);
        if (responseBody.contains("NoSuchKey"))
            return QStringLiteral("The requested log no longer exists.");
        return QStringLiteral("Not found (HTTP 404).");
    }
    if (!networkError.isEmpty() &&
        (networkError.contains(QStringLiteral("timeout"), Qt::CaseInsensitive) ||
         networkError.contains(QStringLiteral("timed out"), Qt::CaseInsensitive))) {
        return QStringLiteral("Network timeout - could not reach %1.").arg(m_config.endpoint);
    }

    // Extract S3 <Code> if present for the generic case.
    QString code;
    QXmlStreamReader xml(responseBody);
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::StartElement &&
            xml.name() == QLatin1String("Code")) {
            code = xml.readElementText();
            break;
        }
    }
    if (!networkError.isEmpty())
        return QStringLiteral("Network error: %1").arg(networkError);
    if (!code.isEmpty())
        return QStringLiteral("Storage error %1 (HTTP %2).").arg(code).arg(httpStatus);
    return QStringLiteral("Unexpected response (HTTP %1).").arg(httpStatus);
}

QString S3StorageWorker::parseListPage(const QByteArray& xml, QVector<CloudObject>& objects) const
{
    QString nextToken;
    QXmlStreamReader r(xml);
    bool truncated = false;
    while (!r.atEnd()) {
        if (r.readNext() != QXmlStreamReader::StartElement)
            continue;
        const auto name = r.name();
        if (name == QLatin1String("Contents")) {
            CloudObject obj;
            while (!(r.tokenType() == QXmlStreamReader::EndElement &&
                     r.name() == QLatin1String("Contents")) && !r.atEnd()) {
                r.readNext();
                if (r.tokenType() == QXmlStreamReader::StartElement) {
                    if (r.name() == QLatin1String("Key"))
                        obj.key = r.readElementText();
                    else if (r.name() == QLatin1String("Size"))
                        obj.size = r.readElementText().toLongLong();
                    else if (r.name() == QLatin1String("LastModified"))
                        obj.lastModified = QDateTime::fromString(r.readElementText(), Qt::ISODate);
                }
            }
            // Strip the configured folder prefix so the UI shows plain file names;
            // skip the folder placeholder entry itself.
            if (!m_prefix.isEmpty() && obj.key.startsWith(m_prefix))
                obj.key = obj.key.mid(m_prefix.length());
            if (!obj.key.isEmpty() && obj.key.endsWith(QLatin1String(".clx"), Qt::CaseInsensitive))
                objects.append(obj);
        } else if (name == QLatin1String("IsTruncated")) {
            truncated = (r.readElementText().compare(QLatin1String("true"), Qt::CaseInsensitive) == 0);
        } else if (name == QLatin1String("NextContinuationToken")) {
            nextToken = r.readElementText();
        }
    }
    return truncated ? nextToken : QString();
}

void S3StorageWorker::doList()
{
    dlog("List logs: requested");
    QVector<CloudObject> objects;
    QString continuation;
    do {
        QMap<QString, QString> query;
        query.insert(QStringLiteral("list-type"), QStringLiteral("2"));
        if (!m_prefix.isEmpty())
            query.insert(QStringLiteral("prefix"), m_prefix);
        if (!continuation.isEmpty())
            query.insert(QStringLiteral("continuation-token"), continuation);

        QByteArray resp; int status; QString netErr;
        QNetworkReply* reply = sendSigned(QStringLiteral("GET"), QString(), query,
                                          QByteArray(), QString(), resp, status, netErr);
        reply->deleteLater();

        if (status != 200) {
            emit operationFailed(CloudOp::List, classifyError(status, netErr, resp));
            return;
        }
        continuation = parseListPage(resp, objects);
    } while (!continuation.isEmpty());

    dlog(QString("List logs: %1 .clx object(s) found").arg(objects.size()));
    emit listReady(objects);
}

void S3StorageWorker::doDownload(const QString& key, const QString& localPath)
{
    dlog(QString("Download: key=%1").arg(key));
    QByteArray resp; int status; QString netErr;
    QNetworkReply* reply = sendSigned(QStringLiteral("GET"), key, {},
                                      QByteArray(), QString(), resp, status, netErr);
    reply->deleteLater();

    if (status != 200) {
        emit operationFailed(CloudOp::Download, classifyError(status, netErr, resp));
        return;
    }
    QFile f(localPath);
    if (!f.open(QIODevice::WriteOnly)) {
        emit operationFailed(CloudOp::Download,
                             QStringLiteral("Could not write the downloaded log locally."));
        return;
    }
    f.write(resp);
    f.close();
    dlog(QString("Download: key=%1 ok (%2 bytes)").arg(key).arg(resp.size()));
    emit downloadReady(key, localPath);
}

void S3StorageWorker::doUpload(const QString& key, const QString& localPath)
{
    QFile f(localPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit operationFailed(CloudOp::Upload,
                             QStringLiteral("Could not read the local log for upload."));
        return;
    }
    const QByteArray body = f.readAll();
    f.close();

    dlog(QString("Upload: key=%1 (%2 bytes)").arg(key).arg(body.size()));

    QByteArray resp; int status; QString netErr;
    QNetworkReply* reply = sendSigned(QStringLiteral("PUT"), key, {}, body,
                                      QStringLiteral("application/json"), resp, status, netErr);
    reply->deleteLater();

    if (status != 200 && status != 204) {
        emit operationFailed(CloudOp::Upload, classifyError(status, netErr, resp));
        return;
    }
    dlog(QString("Upload: key=%1 ok").arg(key));
    emit uploadFinished(key);
}

void S3StorageWorker::doDelete(const QString& key)
{
    dlog(QString("Delete: key=%1").arg(key));
    QByteArray resp; int status; QString netErr;
    QNetworkReply* reply = sendSigned(QStringLiteral("DELETE"), key, {},
                                      QByteArray(), QString(), resp, status, netErr);
    reply->deleteLater();

    if (status != 200 && status != 204) {
        emit operationFailed(CloudOp::Delete, classifyError(status, netErr, resp));
        return;
    }
    emit deleteFinished(key);
}

void S3StorageWorker::doCheckExists(const QString& key)
{
    QByteArray resp; int status; QString netErr;
    QNetworkReply* reply = sendSigned(QStringLiteral("HEAD"), key, {},
                                      QByteArray(), QString(), resp, status, netErr);
    reply->deleteLater();

    if (status == 200) {
        dlog(QString("Exists check: key=%1 -> exists").arg(key));
        emit objectExists(key, true);
    } else if (status == 404) {
        dlog(QString("Exists check: key=%1 -> not found").arg(key));
        emit objectExists(key, false);
    } else {
        emit operationFailed(CloudOp::Exists, classifyError(status, netErr, resp));
    }
}

void S3StorageWorker::doTest()
{
    dlog("Test connection: requested");
    // Lightweight: list with a small max-keys; success means creds + bucket are good.
    QMap<QString, QString> query;
    query.insert(QStringLiteral("list-type"), QStringLiteral("2"));
    query.insert(QStringLiteral("max-keys"), QStringLiteral("1"));
    if (!m_prefix.isEmpty())
        query.insert(QStringLiteral("prefix"), m_prefix);

    QByteArray resp; int status; QString netErr;
    QNetworkReply* reply = sendSigned(QStringLiteral("GET"), QString(), query,
                                      QByteArray(), QString(), resp, status, netErr);
    reply->deleteLater();

    const bool ok = (status == 200);
    dlog(QString("Test connection: %1").arg(ok ? "OK" : "failed"));
    if (ok)
        emit testResult(true, QStringLiteral("Connection OK."));
    else
        emit testResult(false, classifyError(status, netErr, resp));
}
