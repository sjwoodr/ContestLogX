/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * HttpServer - embedded HTTP/1.1 server for the Remote Control feature
 * (TODO item 3). Runs in the main Qt event loop on QTcpServer; each
 * connection is one short-lived request+response (close after response,
 * no keep-alive). Bodies are always JSON for /api/* routes; the root
 * path serves the static mobile dashboard page.
 *
 * Authentication: every request must supply the bearer token either
 * in the "Authorization: Bearer <token>" header or in a "?token=<token>"
 * query param. Dashboard HTML uses query-param auth so the URL is
 * bookmarkable on a phone home screen.
 *
 * Threading: handlers run on the Qt main thread. Data comes from a
 * thread-safe ClxSnapshot that MainWindow updates on its own cadence;
 * handlers take a cheap Copy of the snapshot and serialize without
 * holding any lock.
 */

#ifndef NET_HTTPSERVER_H
#define NET_HTTPSERVER_H

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>

class QTcpServer;
class QTcpSocket;

namespace clx::net {

class ClxSnapshot;

struct HttpRequest {
    QString method;                 // "GET", "POST"
    QString path;                   // "/api/status"
    QHash<QString, QString> query;  // decoded ?k=v&k2=v2
    QHash<QString, QString> headers;
    QByteArray body;
};

struct HttpResponse {
    int status = 200;
    QString contentType = QStringLiteral("application/json");
    QByteArray body;
    QHash<QString, QString> extraHeaders;
};

// Handler signature - simple synchronous request→response. Register via
// registerRoute(). Handlers run on the Qt main thread so they can safely
// read from the snapshot (which takes its own lock internally).
using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer : public QObject {
    Q_OBJECT
public:
    explicit HttpServer(ClxSnapshot* snapshot, QObject* parent = nullptr);
    ~HttpServer() override;

    // Start listening on the configured port + bind mode. Reads port,
    // token, and bind mode from Settings. No-op if already running.
    bool start();

    // Stop accepting new connections. Drains open sockets. No-op if
    // already stopped.
    void stop();

    bool isRunning() const { return m_running; }

    // The address the server is actually listening on (useful for
    // showing "http://<ip>:<port>" in Preferences after start).
    QString listenAddress() const { return m_listenAddress; }
    int     listenPort() const    { return m_listenPort; }

    // Register a route. Thread-safe to call only before start() (or after
    // stop()). Method is "GET" or "POST". Path is exact-match (no glob).
    // The handler receives a parsed HttpRequest and returns an HttpResponse.
    void registerRoute(const QString& method, const QString& path, HttpHandler handler);

signals:
    // Emitted on any accepted (authenticated + routed) request, useful
    // for a debug log.
    void requestServed(const QString& method, const QString& path, int status);
    void errorOccurred(const QString& message);

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();

private:
    // Parse buffered bytes into HttpRequest. Returns false if more data
    // is needed (handler call deferred); true if a complete request was
    // parsed (or malformed - in which case responds 400 immediately).
    bool tryParseAndDispatch(QTcpSocket* socket, QByteArray& buf);

    HttpResponse authenticate(const HttpRequest& req);
    void writeResponse(QTcpSocket* socket, const HttpResponse& resp);
    QString chooseBindAddress(const QString& mode) const;

    ClxSnapshot* m_snapshot = nullptr;   // non-owning
    QTcpServer*  m_server   = nullptr;

    struct RouteKey {
        QString method;
        QString path;
        bool operator==(const RouteKey& o) const noexcept {
            return method == o.method && path == o.path;
        }
    };
    friend uint qHash(const RouteKey& k, uint seed) noexcept;
    QHash<RouteKey, HttpHandler> m_routes;

    // Per-socket read buffer - HTTP requests can arrive fragmented.
    QHash<QTcpSocket*, QByteArray> m_socketBuffers;

    bool m_running = false;
    QString m_listenAddress;
    int     m_listenPort = 0;
    QString m_token;   // cached from Settings at start()
};

} // namespace clx::net

#endif // NET_HTTPSERVER_H
