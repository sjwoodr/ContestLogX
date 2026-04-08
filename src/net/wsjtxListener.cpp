#include "wsjtxListener.h"
#include "debugLogger.h"
#include <QNetworkDatagram>

WsjtxListener::WsjtxListener(QObject *parent)
    : QObject(parent)
{
}

WsjtxListener::~WsjtxListener()
{
    stopListening();
}

bool WsjtxListener::startListening(int port)
{
    stopListening();

    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress::Any, port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        QString err = QString("Failed to bind WSJT-X UDP port %1: %2").arg(port).arg(m_socket->errorString());
        DebugLogger::instance().log("WsjtxListener", err);
        emit error(err);
        delete m_socket;
        m_socket = nullptr;
        return false;
    }

    m_port = port;
    connect(m_socket, &QUdpSocket::readyRead, this, &WsjtxListener::onReadyRead);

    DebugLogger::instance().log("WsjtxListener", QString("Listening on UDP port %1").arg(port));
    emit listening(port);
    return true;
}

void WsjtxListener::stopListening()
{
    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
        m_port = 0;
        DebugLogger::instance().log("WsjtxListener", "Stopped listening");
        emit stopped();
    }
}

bool WsjtxListener::isListening() const
{
    return m_socket != nullptr && m_socket->state() == QAbstractSocket::BoundState;
}

int WsjtxListener::port() const
{
    return m_port;
}

void WsjtxListener::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();
        QDataStream stream(data);
        stream.setByteOrder(QDataStream::BigEndian);

        quint32 messageType = 0;
        QString clientId;
        if (!parseHeader(stream, messageType, clientId))
            continue;

        if (messageType == MSG_QSO_LOGGED) {
            WsjtxQsoData qsoData;
            if (parseQsoLogged(stream, qsoData)) {
                DebugLogger::instance().log("WsjtxListener",
                    QString("QSO logged from %1: %2 on %3 Hz %4")
                        .arg(clientId, qsoData.callsign)
                        .arg(qsoData.frequencyHz)
                        .arg(qsoData.mode));
                emit qsoReceived(qsoData);
            }
        }
    }
}

bool WsjtxListener::parseHeader(QDataStream& stream, quint32& messageType, QString& clientId)
{
    quint32 magic = 0;
    quint32 schema = 0;

    stream >> magic;
    if (magic != WSJTX_MAGIC)
        return false;

    stream >> schema >> messageType;
    clientId = readUtf8(stream);

    return stream.status() == QDataStream::Ok;
}

bool WsjtxListener::parseQsoLogged(QDataStream& stream, WsjtxQsoData& data)
{
    // Type 5: QSO Logged
    // Fields: timeOff, dxCall, dxGrid, txFreq, mode, reportSent, reportReceived,
    //         txPower, comments, name, timeOn, operatorCall, deCall, deGrid,
    //         exchangeSent, exchangeReceived, propagationMode

    data.timeOff = readDateTime(stream);
    data.callsign = readUtf8(stream);
    data.gridSquare = readUtf8(stream);

    stream >> data.frequencyHz;

    data.mode = readUtf8(stream);
    data.reportSent = readUtf8(stream);
    data.reportReceived = readUtf8(stream);

    // txPower and comments — read but don't store
    readUtf8(stream);  // txPower
    readUtf8(stream);  // comments

    data.operatorName = readUtf8(stream);
    data.timeOn = readDateTime(stream);

    // Schema 2+ fields — read if available
    if (stream.status() == QDataStream::Ok) {
        readUtf8(stream);  // operatorCall
        readUtf8(stream);  // deCall
        readUtf8(stream);  // deGrid
        data.exchangeSent = readUtf8(stream);
        data.exchangeReceived = readUtf8(stream);
        // propagationMode — optional, ignore
    }

    return !data.callsign.isEmpty();
}

QString WsjtxListener::readUtf8(QDataStream& stream)
{
    // QDataStream serializes QString as: quint32 byte-length + UTF-16BE data
    // A null QString is encoded as 0xFFFFFFFF
    QString result;
    stream >> result;
    return result;
}

QDateTime WsjtxListener::readDateTime(QDataStream& stream)
{
    // QDataStream serializes QDateTime as: QDate + QTime + timespec
    QDateTime dt;
    stream >> dt;
    return dt;
}
