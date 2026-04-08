#ifndef WSJTXLISTENER_H
#define WSJTXLISTENER_H

#include <QObject>
#include <QUdpSocket>
#include <QDateTime>

struct WsjtxQsoData {
    QString callsign;
    QString gridSquare;
    quint64 frequencyHz = 0;
    QString mode;
    QString reportSent;
    QString reportReceived;
    QString exchangeSent;
    QString exchangeReceived;
    QString operatorName;
    QDateTime timeOn;
    QDateTime timeOff;
};

class WsjtxListener : public QObject
{
    Q_OBJECT

public:
    explicit WsjtxListener(QObject *parent = nullptr);
    ~WsjtxListener();

    bool startListening(int port = 2237);
    void stopListening();
    bool isListening() const;
    int port() const;

signals:
    void qsoReceived(const WsjtxQsoData& data);
    void listening(int port);
    void stopped();
    void error(const QString& errorString);

private slots:
    void onReadyRead();

private:
    static constexpr quint32 WSJTX_MAGIC = 0xADBCCBDA;
    static constexpr quint32 MSG_HEARTBEAT = 0;
    static constexpr quint32 MSG_STATUS = 1;
    static constexpr quint32 MSG_DECODE = 2;
    static constexpr quint32 MSG_QSO_LOGGED = 5;
    static constexpr quint32 MSG_ADIF_LOGGED = 12;

    bool parseHeader(QDataStream& stream, quint32& messageType, QString& clientId);
    bool parseQsoLogged(QDataStream& stream, WsjtxQsoData& data);
    QString readUtf8(QDataStream& stream);
    QDateTime readDateTime(QDataStream& stream);

    QUdpSocket *m_socket = nullptr;
    int m_port = 0;
};

#endif // WSJTXLISTENER_H
