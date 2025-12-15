/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
 */

#include "flrigclient.h"
#include "debuglogger.h"
#include <QDebug>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QThread>


FlrigClient::FlrigClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_host("localhost")
    , m_port(12345)
{
    QObject::connect(m_socket, &QTcpSocket::connected, this, &FlrigClient::onSocketConnected);
    QObject::connect(m_socket, &QTcpSocket::disconnected, this, &FlrigClient::onSocketDisconnected);
    QObject::connect(m_socket, &QTcpSocket::errorOccurred, this, &FlrigClient::onSocketError);
    QObject::connect(m_socket, &QTcpSocket::readyRead, this, &FlrigClient::onReadyRead);
}

FlrigClient::~FlrigClient()
{
    disconnect();
}

bool FlrigClient::connectToRig(const QString& host, int port)
{
    m_host = host;
    m_port = port;
    
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        return true;
    }
    
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Connecting to flrig at %1:%2").arg(host).arg(port).toStdString().c_str());
    m_socket->connectToHost(host, port);
    return m_socket->waitForConnected(3000);
}

void FlrigClient::disconnectFromRig()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
    }
}

bool FlrigClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

QString FlrigClient::getMode()
{
    if (!isConnected()) {
        if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", "getMode() called but not connected");
        return QString();
    }
    
    QString request = buildXmlRpcCall("rig.get_mode");
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Sending mode request: %1").arg(request).toStdString().c_str());
    sendRequest(request);
    
    if (m_socket->waitForReadyRead(1000)) {
        if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Response buffer: %1").arg(m_responseBuffer).toStdString().c_str());
        QVariant result = parseXmlRpcResponse(m_responseBuffer);
        m_responseBuffer.clear();
        if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Parsed mode: %1").arg(result.toString()).toStdString().c_str());
        return result.toString();
    }
    
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", "Mode request timeout");
    return QString();
}

bool FlrigClient::setMode(const QString& mode)
{
    if (!isConnected())
        return false;
    
    QVariantList params;
    params << mode;
    
    QString request = buildXmlRpcCall("rig.set_mode", params);
    sendRequest(request);
    
    if (m_socket->waitForReadyRead(1000)) {
        m_responseBuffer.clear();
        return true;
    }
    
    return false;
}

double FlrigClient::getFrequency()
{
    if (!isConnected()) {
        if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", "getFrequency() called but not connected");
        return 0.0;
    }
    
    QString request = buildXmlRpcCall("rig.get_vfo");
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Sending frequency request: %1").arg(request).toStdString().c_str());
    sendRequest(request);
    
    if (m_socket->waitForReadyRead(1000)) {
        if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Response buffer: %1").arg(m_responseBuffer).toStdString().c_str());
        QVariant result = parseXmlRpcResponse(m_responseBuffer);
        m_responseBuffer.clear();
        if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Parsed frequency: %1").arg(result.toDouble()).toStdString().c_str());
        return result.toDouble();
    }
    
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", "Frequency request timeout");
    return 0.0;
}

bool FlrigClient::setFrequency(double freqHz)
{
    if (!isConnected())
        return false;
    
    QVariantList params;
    params << freqHz;
    
    QString request = buildXmlRpcCall("rig.set_vfo", params);
    sendRequest(request);
    
    if (m_socket->waitForReadyRead(1000)) {
        m_responseBuffer.clear();
        emit frequencyChanged(freqHz);
        return true;
    }
    
    return false;
}

QString FlrigClient::getRigName()
{
    if (!isConnected())
        return QString();
    
    QString request = buildXmlRpcCall("rig.get_xcvr");
    sendRequest(request);
    
    if (m_socket->waitForReadyRead(1000)) {
        QVariant result = parseXmlRpcResponse(m_responseBuffer);
        m_responseBuffer.clear();
        return result.toString();
    }
    
    return QString();
}

bool FlrigClient::stopCW()
{
    DebugLogger::instance().log("CW", "stopCW called - clearing CW buffer");
    
    if (!isConnected()) {
        DebugLogger::instance().log("CW", "Not connected, cannot stop");
        return false;
    }
    
    // Send rig.cwio_send with parameter 0 to stop/clear the buffer
    QVariantList params;
    params << 0;
    
    QString request = buildXmlRpcCall("rig.cwio_send", params);
    DebugLogger::instance().log("CW", QString("Sending stop command: %1").arg(request));
    
    QByteArray httpRequest = QString(
        "POST /RPC2 HTTP/1.1\r\n"
        "Host: %1:%2\r\n"
        "Content-Type: text/xml\r\n"
        "Content-Length: %3\r\n"
        "\r\n%4"
    ).arg(m_host).arg(m_port).arg(request.length()).arg(request).toUtf8();
    
    m_socket->write(httpRequest);
    m_socket->flush();
    
    DebugLogger::instance().log("CW", "Stop command sent");
    return true;
}

bool FlrigClient::sendCW(const QString& text)
{
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString(">>> sendCW ENTRY: text=%1 length=%2").arg(text).arg(text.length()).toStdString().c_str());
    DebugLogger::instance().log("CW", QString("sendCW called with text: \"%1\"").arg(text));
    
    if (!isConnected()) {
        if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", ">>> sendCW: Not connected");
        DebugLogger::instance().log("CW", "Not connected, cannot send");
        return false;
    }
    
    if (text.isEmpty()) {
        DebugLogger::instance().log("CW", "Empty text, nothing to send");
        return false;
    }
    
    // First, clear any pending CW in the buffer
    stopCW();
    QThread::msleep(100);  // Give flrig time to clear the buffer
    
    // Format text for flrig's cwio_text according to QLog's implementation
    // '[' starts CW sending, ']' stops it
    QString cwText = text.toUpper(); // Convert to uppercase for CW
    QString formattedText = "[" + cwText + "]";
    
    // FIXME: if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", (">>> sendCW: Original text:" << text << "Formatted:" << formattedText).toStdString().c_str());
    DebugLogger::instance().log("CW", QString("Sending CW text: \"%1\" formatted as: \"%2\"").arg(cwText).arg(formattedText));
    
    // Send using rig.cwio_text with bracket formatting
    QVariantList params;
    params << formattedText;
    
    QString textRequest = buildXmlRpcCall("rig.cwio_text", params);
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString(">>> sendCW: XML-RPC request: %1").arg(textRequest).toStdString().c_str());
    DebugLogger::instance().log("CW", QString("XML-RPC request: %1").arg(textRequest));
    
    QByteArray httpTextRequest = QString(
        "POST /RPC2 HTTP/1.1\r\n"
        "Host: %1:%2\r\n"
        "Content-Type: text/xml\r\n"
        "Content-Length: %3\r\n"
        "\r\n%4"
    ).arg(m_host).arg(m_port).arg(textRequest.length()).arg(textRequest).toUtf8();
    
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", ">>> sendCW: Writing to socket");
    m_socket->write(httpTextRequest);
    m_socket->flush();
    
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", ">>> sendCW: DONE, returning true");
    DebugLogger::instance().log("CW", "CW command sent");
    return true;
}

void FlrigClient::onSocketConnected()
{
    // FIXME: if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", ("Connected to flrig at" << m_host << ":" << m_port).toStdString().c_str());
    emit connected();
}

void FlrigClient::onSocketDisconnected()
{
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", "Disconnected from flrig");
    emit disconnected();
}

void FlrigClient::onSocketError(QAbstractSocket::SocketError socketError)
{
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Socket error: %1").arg(m_socket->errorString()).toStdString().c_str());
    emit error(m_socket->errorString());
}

void FlrigClient::onReadyRead()
{
    m_responseBuffer += QString::fromUtf8(m_socket->readAll());
    
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Received data: %1").arg(m_responseBuffer).toStdString().c_str());
    
    // Check if we have a complete response (ends with </methodResponse>)
    if (m_responseBuffer.contains("</methodResponse>")) {
        // Response is complete, will be processed by caller
        if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", "Complete response received");
    }
}

QString FlrigClient::buildXmlRpcCall(const QString& method, const QVariantList& params)
{
    QString xml = "<?xml version=\"1.0\"?>\r\n";
    xml += "<methodCall>\r\n";
    xml += "  <methodName>" + method + "</methodName>\r\n";
    
    if (!params.isEmpty()) {
        xml += "  <params>\r\n";
        
        for (const QVariant& param : params) {
            xml += "    <param><value>";
            
            QMetaType::Type typeId = static_cast<QMetaType::Type>(param.typeId());
            switch (typeId) {
                case QMetaType::Int:
                case QMetaType::LongLong:
                    xml += "<int>" + param.toString() + "</int>";
                    break;
                case QMetaType::Double:
                    xml += "<double>" + QString::number(param.toDouble(), 'f', 1) + "</double>";
                    break;
                case QMetaType::Bool:
                    xml += "<boolean>" + QString::number(param.toBool() ? 1 : 0) + "</boolean>";
                    break;
                default:
                    xml += "<string>" + param.toString() + "</string>";
                    break;
            }
            
            xml += "</value></param>\r\n";
        }
        
        xml += "  </params>\r\n";
    }
    
    xml += "</methodCall>\r\n";
    return xml;
}

QVariant FlrigClient::parseXmlRpcResponse(const QString& xml)
{
    // Find the response body (skip HTTP headers)
    int bodyStart = xml.indexOf("<?xml");
    if (bodyStart == -1) {
        bodyStart = xml.indexOf("<methodResponse");
    }
    
    QString body = (bodyStart >= 0) ? xml.mid(bodyStart) : xml;
    
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Parsing XML body: %1").arg(body).toStdString().c_str());
    
    QXmlStreamReader reader(body);
    QVariant result;
    bool inValue = false;
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isStartElement()) {
            QString name = reader.name().toString();
            
            // Track when we're inside a <value> tag
            if (name == "value") {
                inValue = true;
                continue;
            }
            
            // Parse typed values
            if (name == "double" || name == "int" || name == "i4") {
                QString text = reader.readElementText();
                if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Found typed number: %1").arg(text).toStdString().c_str());
                result = text.toDouble();
            }
            else if (name == "string") {
                QString text = reader.readElementText();
                if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Found typed string: %1").arg(text).toStdString().c_str());
                result = text;
            }
            else if (name == "boolean") {
                QString text = reader.readElementText();
                if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Found boolean: %1").arg(text).toStdString().c_str());
                result = (text == "1" || text.toLower() == "true");
            }
            else if (name == "fault") {
                qWarning() << "XML-RPC fault in response";
                return QVariant();
            }
        }
        else if (reader.isCharacters() && inValue && !reader.isWhitespace()) {
            // Handle untyped values (flrig doesn't always use type tags!)
            QString text = reader.text().toString().trimmed();
            if (!text.isEmpty()) {
                if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Found untyped value: %1").arg(text).toStdString().c_str());
                // Try to determine type by content
                bool isNumber;
                double numValue = text.toDouble(&isNumber);
                if (isNumber) {
                    result = numValue;
                    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Parsed as number: %1").arg(numValue).toStdString().c_str());
                } else {
                    result = text;
                    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Parsed as string: %1").arg(text).toStdString().c_str());
                }
            }
        }
        else if (reader.isEndElement() && reader.name() == QStringLiteral("value")) {
            inValue = false;
        }
    }
    
    if (reader.hasError()) {
        qWarning() << "XML parsing error:" << reader.errorString();
        return QVariant();
    }
    
    if (DebugLogger::instance().isFlrigDebugEnabled()) DebugLogger::instance().log("Flrig", QString("Final parsed result: %1").arg(result.toString()).toStdString().c_str());
    return result;
}

void FlrigClient::sendRequest(const QString& xmlRequest)
{
    if (!isConnected()) {
        qWarning() << "Cannot send request - not connected";
        return;
    }
    
    // HTTP POST wrapper for XML-RPC
    QString httpRequest = QString(
        "POST /RPC2 HTTP/1.1\r\n"
        "Host: %1:%2\r\n"
        "Content-Type: text/xml\r\n"
        "Content-Length: %3\r\n"
        "\r\n"
        "%4"
    ).arg(m_host).arg(m_port).arg(xmlRequest.length()).arg(xmlRequest);
    
    m_socket->write(httpRequest.toUtf8());
    m_socket->flush();
}

int FlrigClient::getCWSpeed()
{
    // flrig doesn't provide a method to get CW speed for cwio
    // This is controlled locally by timing between character sends
    return 0;
}

bool FlrigClient::setCWSpeed(int wpm)
{
    DebugLogger::instance().log("FlrigClient", QString("setCWSpeed called with WPM: %1").arg(wpm));
    
    if (!isConnected()) {
        DebugLogger::instance().log("FlrigClient", "Not connected, cannot set CW speed");
        return false;
    }
    
    if (wpm < 5 || wpm > 60) {
        DebugLogger::instance().log("FlrigClient", QString("Invalid WPM value: %1 (must be 5-60)").arg(wpm));
        return false;
    }
    
    QVariantList params;
    params << wpm;
    
    QString request = buildXmlRpcCall("rig.cwio_set_wpm", params);
    DebugLogger::instance().log("FlrigClient", QString("Sending cwio_set_wpm request: %1").arg(request));
    
    QByteArray httpRequest = QString(
        "POST /RPC2 HTTP/1.1\r\n"
        "Host: %1:%2\r\n"
        "Content-Type: text/xml\r\n"
        "Content-Length: %3\r\n"
        "\r\n%4"
    ).arg(m_host).arg(m_port).arg(request.length()).arg(request).toUtf8();
    
    m_socket->write(httpRequest);
    m_socket->flush();
    
    DebugLogger::instance().log("FlrigClient", QString("CW speed set to %1 WPM via cwio_set_wpm").arg(wpm));
    return true;
}

bool FlrigClient::getPTT()
{
    if (!isConnected()) {
        return false;
    }
    
    QString request = buildXmlRpcCall("rig.get_ptt");
    sendRequest(request);
    
    if (!m_socket->waitForReadyRead(1000)) {
        return false;
    }
    
    QVariant response = parseXmlRpcResponse(m_responseBuffer);
    m_responseBuffer.clear();
    
    return response.toInt() == 1;
}

bool FlrigClient::setPTT(bool enable)
{
    if (!isConnected()) {
        return false;
    }
    
    QVariantList params;
    params << (enable ? 1 : 0);
    
    QString request = buildXmlRpcCall("rig.set_ptt", params);
    sendRequest(request);
    
    DebugLogger::instance().log("FlrigClient", QString("PTT set to %1").arg(enable ? "ON" : "OFF"));
    return true;
}

int FlrigClient::getPower()
{
    if (!isConnected()) {
        return 0;
    }
    
    QString request = buildXmlRpcCall("rig.get_power");
    sendRequest(request);
    
    if (!m_socket->waitForReadyRead(1000)) {
        return 0;
    }
    
    QVariant response = parseXmlRpcResponse(m_responseBuffer);
    m_responseBuffer.clear();
    
    return response.toInt();
}

bool FlrigClient::setPower(int watts)
{
    if (!isConnected()) {
        return false;
    }
    
    QVariantList params;
    params << watts;
    
    QString request = buildXmlRpcCall("rig.set_power", params);
    sendRequest(request);
    
    DebugLogger::instance().log("FlrigClient", QString("Power set to %1 watts").arg(watts));
    return true;
}

int FlrigClient::getBandwidth()
{
    if (!isConnected()) {
        return 0;
    }
    
    QString request = buildXmlRpcCall("rig.get_bw");
    sendRequest(request);
    
    if (!m_socket->waitForReadyRead(1000)) {
        return 0;
    }
    
    QVariant response = parseXmlRpcResponse(m_responseBuffer);
    m_responseBuffer.clear();
    
    return response.toInt();
}

bool FlrigClient::setBandwidth(int hz)
{
    if (!isConnected()) {
        return false;
    }
    
    QVariantList params;
    params << hz;
    
    QString request = buildXmlRpcCall("rig.set_bw", params);
    sendRequest(request);
    
    DebugLogger::instance().log("FlrigClient", QString("Bandwidth set to %1 Hz").arg(hz));
    return true;
}

QString FlrigClient::getVFO()
{
    if (!isConnected()) {
        return QString();
    }
    
    QString request = buildXmlRpcCall("rig.get_AB");
    sendRequest(request);
    
    if (!m_socket->waitForReadyRead(1000)) {
        return QString();
    }
    
    QVariant response = parseXmlRpcResponse(m_responseBuffer);
    m_responseBuffer.clear();
    
    return response.toString();
}

bool FlrigClient::setVFO(const QString& vfo)
{
    if (!isConnected()) {
        return false;
    }
    
    if (vfo != "A" && vfo != "B") {
        DebugLogger::instance().log("FlrigClient", QString("Invalid VFO: %1 (must be A or B)").arg(vfo));
        return false;
    }
    
    QVariantList params;
    params << vfo;
    
    QString request = buildXmlRpcCall("rig.set_AB", params);
    sendRequest(request);
    
    DebugLogger::instance().log("FlrigClient", QString("VFO set to %1").arg(vfo));
    return true;
}
