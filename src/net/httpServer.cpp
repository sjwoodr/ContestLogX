/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "net/httpServer.h"
#include "net/clxSnapshot.h"
#include "settings.h"
#include "debugLogger.h"

#include <QDateTime>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

namespace clx::net {

uint qHash(const HttpServer::RouteKey& k, uint seed) noexcept
{
    return qHash(k.method, seed) ^ qHash(k.path, seed);
}

namespace {
constexpr int kMaxRequestBytes = 64 * 1024;   // plenty for our JSON payloads
constexpr int kHeaderTerminator = 4;          // "\r\n\r\n"

QByteArray httpStatusLine(int status)
{
    switch (status) {
    case 200: return "HTTP/1.1 200 OK\r\n";
    case 400: return "HTTP/1.1 400 Bad Request\r\n";
    case 401: return "HTTP/1.1 401 Unauthorized\r\n";
    case 404: return "HTTP/1.1 404 Not Found\r\n";
    case 405: return "HTTP/1.1 405 Method Not Allowed\r\n";
    case 413: return "HTTP/1.1 413 Payload Too Large\r\n";
    case 500: return "HTTP/1.1 500 Internal Server Error\r\n";
    default:  return QByteArray("HTTP/1.1 ") + QByteArray::number(status) + " \r\n";
    }
}

QByteArray jsonError(const QString& msg)
{
    return QByteArray("{\"error\":\"") + msg.toUtf8().replace('"', "\\\"") + "\"}";
}
} // namespace

HttpServer::HttpServer(ClxSnapshot* snapshot, QObject* parent)
    : QObject(parent)
    , m_snapshot(snapshot)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
            this, &HttpServer::onNewConnection);
}

HttpServer::~HttpServer()
{
    stop();
}

void HttpServer::registerRoute(const QString& method, const QString& path, HttpHandler handler)
{
    m_routes.insert({method.toUpper(), path}, std::move(handler));
}

QString HttpServer::chooseBindAddress(const QString& mode) const
{
    if (mode == QLatin1String("localhost")) {
        return QStringLiteral("127.0.0.1");
    }
    if (mode == QLatin1String("any")) {
        return QStringLiteral("0.0.0.0");
    }
    // Default "lan": pick the first non-loopback IPv4 address on an up
    // interface. If none found, fall back to 0.0.0.0 so the server at
    // least listens somewhere usable.
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            const QHostAddress addr = entry.ip();
            if (addr.protocol() == QAbstractSocket::IPv4Protocol
                && !addr.isLoopback()) {
                return addr.toString();
            }
        }
    }
    return QStringLiteral("0.0.0.0");
}

bool HttpServer::start()
{
    if (m_running) return true;

    Settings& s = Settings::instance();
    m_token = s.getRemoteControlToken();
    if (m_token.isEmpty()) {
        emit errorOccurred(QStringLiteral("Remote Control: no auth token configured; "
                                          "enable in Preferences first"));
        return false;
    }

    const int port = s.getRemoteControlPort();
    const QString bindMode = s.getRemoteControlBindMode();
    const QString addrStr = chooseBindAddress(bindMode);
    const QHostAddress addr = (addrStr == QLatin1String("0.0.0.0"))
        ? QHostAddress(QHostAddress::Any)
        : QHostAddress(addrStr);

    if (!m_server->listen(addr, static_cast<quint16>(port))) {
        emit errorOccurred(QStringLiteral("Remote Control: listen failed on %1:%2 — %3")
                           .arg(addrStr).arg(port).arg(m_server->errorString()));
        return false;
    }
    m_listenAddress = addrStr;
    m_listenPort = port;
    m_running = true;

    DebugLogger::instance().log("HttpServer",
        QString("Remote Control listening on %1:%2 (bindMode=%3)")
            .arg(addrStr).arg(port).arg(bindMode));
    return true;
}

void HttpServer::stop()
{
    if (!m_running) return;
    m_running = false;
    m_server->close();
    for (auto it = m_socketBuffers.begin(); it != m_socketBuffers.end(); ++it) {
        QTcpSocket* sock = it.key();
        if (sock) {
            sock->disconnectFromHost();
            sock->deleteLater();
        }
    }
    m_socketBuffers.clear();
    DebugLogger::instance().log("HttpServer", "Remote Control stopped");
}

void HttpServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* sock = m_server->nextPendingConnection();
        m_socketBuffers.insert(sock, QByteArray{});
        connect(sock, &QTcpSocket::readyRead, this, &HttpServer::onSocketReadyRead);
        connect(sock, &QTcpSocket::disconnected,
                this, &HttpServer::onSocketDisconnected);
    }
}

void HttpServer::onSocketReadyRead()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    auto it = m_socketBuffers.find(sock);
    if (it == m_socketBuffers.end()) return;

    it->append(sock->readAll());
    if (it->size() > kMaxRequestBytes) {
        HttpResponse r;
        r.status = 413;
        r.body = jsonError(QStringLiteral("request too large"));
        writeResponse(sock, r);
        sock->disconnectFromHost();
        return;
    }

    tryParseAndDispatch(sock, *it);
}

void HttpServer::onSocketDisconnected()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    m_socketBuffers.remove(sock);
    sock->deleteLater();
}

bool HttpServer::tryParseAndDispatch(QTcpSocket* sock, QByteArray& buf)
{
    // Need complete headers before we can proceed.
    const int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0) return false;   // wait for more

    const QByteArray headerPart = buf.left(headerEnd);
    const QList<QByteArray> lines = headerPart.split('\n');
    if (lines.isEmpty()) {
        HttpResponse r; r.status = 400; r.body = jsonError("malformed request");
        writeResponse(sock, r); sock->disconnectFromHost();
        return true;
    }

    // Request line: "METHOD /path?query HTTP/1.1"
    const QByteArray requestLine = lines.first().trimmed();
    const QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 2) {
        HttpResponse r; r.status = 400; r.body = jsonError("malformed request line");
        writeResponse(sock, r); sock->disconnectFromHost();
        return true;
    }

    HttpRequest req;
    req.method = QString::fromUtf8(parts[0]).toUpper();
    const QUrl urlPart(QString::fromUtf8(parts[1]));
    req.path = urlPart.path();
    const QUrlQuery q(urlPart);
    for (const auto& pair : q.queryItems()) {
        req.query.insert(pair.first, pair.second);
    }

    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        if (line.isEmpty()) continue;
        const int colon = line.indexOf(':');
        if (colon < 0) continue;
        const QString k = QString::fromUtf8(line.left(colon)).trimmed().toLower();
        const QString v = QString::fromUtf8(line.mid(colon + 1)).trimmed();
        req.headers.insert(k, v);
    }

    // Body: if Content-Length > 0 and we don't have it yet, wait.
    const int contentLength = req.headers.value(QStringLiteral("content-length"), "0").toInt();
    const int bodyStart = headerEnd + kHeaderTerminator;
    const int haveBody = buf.size() - bodyStart;
    if (haveBody < contentLength) return false;   // wait for more
    req.body = buf.mid(bodyStart, contentLength);

    // Authenticate.
    const HttpResponse authResp = authenticate(req);
    if (authResp.status != 200) {
        writeResponse(sock, authResp);
        emit requestServed(req.method, req.path, authResp.status);
        sock->disconnectFromHost();
        return true;
    }

    // Route.
    HttpResponse resp;
    auto rit = m_routes.find({req.method, req.path});
    if (rit == m_routes.end()) {
        resp.status = 404;
        resp.body = jsonError(QStringLiteral("no route for %1 %2").arg(req.method, req.path));
    } else {
        try {
            resp = rit.value()(req);
        } catch (...) {
            resp.status = 500;
            resp.body = jsonError(QStringLiteral("handler threw"));
        }
    }

    writeResponse(sock, resp);
    emit requestServed(req.method, req.path, resp.status);
    sock->disconnectFromHost();
    return true;
}

HttpResponse HttpServer::authenticate(const HttpRequest& req)
{
    const QString auth = req.headers.value(QStringLiteral("authorization"));
    QString presented;
    if (auth.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive)) {
        presented = auth.mid(7).trimmed();
    }
    if (presented.isEmpty()) {
        presented = req.query.value(QStringLiteral("token"));
    }
    if (presented.isEmpty() || presented != m_token) {
        HttpResponse r;
        r.status = 401;
        r.body = jsonError(QStringLiteral("invalid or missing token"));
        r.extraHeaders.insert(QStringLiteral("WWW-Authenticate"),
                              QStringLiteral("Bearer realm=\"ContestLogX\""));
        return r;
    }
    HttpResponse ok;
    ok.status = 200;
    return ok;
}

void HttpServer::writeResponse(QTcpSocket* sock, const HttpResponse& resp)
{
    QByteArray out;
    out.append(httpStatusLine(resp.status));
    out.append("Content-Type: ");
    out.append(resp.contentType.toUtf8());
    out.append("\r\n");
    out.append("Content-Length: ");
    out.append(QByteArray::number(resp.body.size()));
    out.append("\r\n");
    out.append("Connection: close\r\n");
    out.append("Cache-Control: no-store\r\n");
    for (auto it = resp.extraHeaders.begin(); it != resp.extraHeaders.end(); ++it) {
        out.append(it.key().toUtf8());
        out.append(": ");
        out.append(it.value().toUtf8());
        out.append("\r\n");
    }
    out.append("\r\n");
    out.append(resp.body);
    sock->write(out);
    sock->flush();
}

} // namespace clx::net
