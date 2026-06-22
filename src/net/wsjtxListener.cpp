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

        DebugLogger::instance().log("WsjtxListener",
            QString("Received %1 bytes from %2:%3, hex: %4")
                .arg(data.size())
                .arg(datagram.senderAddress().toString())
                .arg(datagram.senderPort())
                .arg(QString(data.left(40).toHex(' '))));

        QDataStream stream(data);
        stream.setByteOrder(QDataStream::BigEndian);

        quint32 messageType = 0;
        QString clientId;
        if (!parseHeader(stream, messageType, clientId))
            continue;

        if (messageType == MSG_HEARTBEAT) {
            quint32 maxSchema = 0;
            stream >> maxSchema;
            QString version = readUtf8(stream);
            DebugLogger::instance().log("WsjtxListener",
                QString("Heartbeat from %1 v%2 (schema %3)").arg(clientId, version).arg(maxSchema));
        } else if (messageType == MSG_STATUS) {
            quint64 dialFreq = 0;
            stream >> dialFreq;
            QString mode = readUtf8(stream);
            QString dxCall = readUtf8(stream);
            QString report = readUtf8(stream);
            DebugLogger::instance().log("WsjtxListener",
                QString("Status from %1: %2 Hz %3, DX=%4 rpt=%5")
                    .arg(clientId).arg(dialFreq).arg(mode, dxCall, report));
        } else if (messageType == MSG_DECODE) {
            quint8 isNew = 0;
            quint32 time = 0;
            qint32 snr = 0;
            double deltaTime = 0;
            quint32 deltaFreq = 0;
            stream >> isNew >> time >> snr >> deltaTime >> deltaFreq;
            QString decodeMode = readUtf8(stream);
            QString message = readUtf8(stream);
            DebugLogger::instance().log("WsjtxListener",
                QString("Decode from %1: %2 dB %3 Hz %4")
                    .arg(clientId).arg(snr).arg(deltaFreq).arg(message));
        } else if (messageType == MSG_QSO_LOGGED) {
            WsjtxQsoData qsoData;
            if (parseQsoLogged(stream, qsoData)) {
                DebugLogger::instance().log("WsjtxListener",
                    QString("QSO logged from %1: %2 on %3 Hz %4")
                        .arg(clientId, qsoData.callsign)
                        .arg(qsoData.frequencyHz)
                        .arg(qsoData.mode));
                emit qsoReceived(qsoData);
            }
        } else if (messageType == MSG_ADIF_LOGGED) {
            QString adif = readUtf8(stream);
            DebugLogger::instance().log("WsjtxListener",
                QString("ADIF logged from %1: %2").arg(clientId, adif.left(100)));
        } else {
            DebugLogger::instance().log("WsjtxListener",
                QString("Unknown message type %1 from %2 (%3 bytes)").arg(messageType).arg(clientId).arg(data.size()));
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

    // txPower and comments - read but don't store
    readUtf8(stream);  // txPower
    readUtf8(stream);  // comments

    data.operatorName = readUtf8(stream);
    data.timeOn = readDateTime(stream);

    // Schema 2+ fields - read if available
    if (stream.status() == QDataStream::Ok) {
        readUtf8(stream);  // operatorCall
        readUtf8(stream);  // deCall
        readUtf8(stream);  // deGrid
        data.exchangeSent = readUtf8(stream);
        data.exchangeReceived = readUtf8(stream);
        // propagationMode - optional, ignore
    }

    return !data.callsign.isEmpty();
}

QString WsjtxListener::readUtf8(QDataStream& stream)
{
    // WSJT-X encodes strings as QByteArray (quint32 length + raw UTF-8),
    // NOT as Qt's QString (quint32 length + UTF-16BE).
    quint32 length = 0;
    stream >> length;
    if (length == 0xFFFFFFFF || length == 0)
        return QString();
    QByteArray bytes(length, 0);
    stream.readRawData(bytes.data(), length);
    return QString::fromUtf8(bytes);
}

QDateTime WsjtxListener::readDateTime(QDataStream& stream)
{
    // WSJT-X serializes QDateTime as: QDate(qint64 julian) + QTime(quint32 ms) + timespec(quint8)
    qint64 julianDay = 0;
    quint32 msecsSinceMidnight = 0;
    quint8 timeSpec = 0;

    stream >> julianDay >> msecsSinceMidnight >> timeSpec;

    if (julianDay == 0)
        return QDateTime();

    QDate date = QDate::fromJulianDay(julianDay);
    QTime time = QTime::fromMSecsSinceStartOfDay(msecsSinceMidnight);
    return QDateTime(date, time, Qt::UTC);
}
