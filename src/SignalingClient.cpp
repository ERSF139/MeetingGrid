#include "SignalingClient.h"
#include "protocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrl>
#include <QWebSocket>

SignalingClient::SignalingClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QWebSocket())
{
    connect(m_socket, &QWebSocket::connected, this, &SignalingClient::onConnected);
    connect(m_socket, &QWebSocket::textMessageReceived, this, &SignalingClient::onTextMessage);
    connect(m_socket, &QWebSocket::binaryMessageReceived, this, &SignalingClient::onBinaryMessage);
    connect(m_socket, &QWebSocket::disconnected, this, &SignalingClient::onDisconnected);
    // 连接阶段出错（连不上/握手失败）时上报，方便界面提示
    connect(m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (!isConnected())
            emit errorOccurred(QStringLiteral("connect_failed"), m_socket->errorString());
    });
}

SignalingClient::~SignalingClient()
{
    m_socket->abort();
    delete m_socket;
}

bool SignalingClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

QString SignalingClient::roomId() const { return m_roomId; }
QString SignalingClient::selfId() const { return m_selfId; }

void SignalingClient::connectToServer(const QString &url)
{
    m_roomId.clear();
    m_selfId.clear();
    m_socket->open(QUrl(url));
}

void SignalingClient::disconnectFromServer()
{
    m_roomId.clear();
    m_selfId.clear();
    m_socket->close();
}

// ---------------------------------------------------------------------------
// 协议操作
// ---------------------------------------------------------------------------

void SignalingClient::createRoom(const QString &name)
{
    QJsonObject obj;
    obj[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::C2S::CreateRoom);
    obj[QLatin1String(Protocol::KeyName)] = name;
    sendJson(obj);
}

void SignalingClient::joinRoom(const QString &roomId, const QString &name)
{
    QJsonObject obj;
    obj[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::C2S::JoinRoom);
    obj[QLatin1String(Protocol::KeyRoomId)] = roomId;
    obj[QLatin1String(Protocol::KeyName)] = name;
    sendJson(obj);
}

void SignalingClient::leaveRoom()
{
    QJsonObject obj;
    obj[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::C2S::LeaveRoom);
    sendJson(obj);
}

void SignalingClient::setState(bool mic, bool cam)
{
    QJsonObject obj;
    obj[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::C2S::SetState);
    obj[QLatin1String(Protocol::KeyMic)] = mic;
    obj[QLatin1String(Protocol::KeyCam)] = cam;
    sendJson(obj);
}

void SignalingClient::setShare(bool on)
{
    QJsonObject obj;
    obj[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::C2S::SetShare);
    obj[QLatin1String(Protocol::KeyOn)] = on;
    sendJson(obj);
}

void SignalingClient::sendChat(const QString &text)
{
    QJsonObject obj;
    obj[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::C2S::Chat);
    obj[QLatin1String(Protocol::KeyText)] = text;
    sendJson(obj);
}

void SignalingClient::ping()
{
    QJsonObject obj;
    obj[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::C2S::Ping);
    sendJson(obj);
}

void SignalingClient::sendMedia(const QJsonObject &meta, const QByteArray &payload)
{
    if (!isConnected())
        return;
    m_socket->sendBinaryMessage(packEnvelope(meta, payload));
}

// ---------------------------------------------------------------------------
// 服务端消息解析
// ---------------------------------------------------------------------------

void SignalingClient::onConnected()
{
    emit connected();
}

void SignalingClient::onDisconnected()
{
    m_roomId.clear();
    m_selfId.clear();
    emit disconnected();
}

void SignalingClient::onBinaryMessage(const QByteArray &data)
{
    QJsonObject header;
    QByteArray payload;
    if (!unpackEnvelope(data, header, payload))
        return;

    const QString kind = header.value(QLatin1String(Protocol::KeyKind)).toString();
    const QString senderId = header.value(QLatin1String(Protocol::KeyMemberId)).toString();
    if (kind.isEmpty())
        return;
    emit mediaFrameReceived(kind, senderId, payload, header);
}

void SignalingClient::onTextMessage(const QString &message)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    const QJsonObject obj = doc.object();
    const QString type = obj.value(QLatin1String(Protocol::KeyType)).toString();

    if (type == QLatin1String(Protocol::S2C::Hello)) {
        // 建连问候，无需处理
    } else if (type == QLatin1String(Protocol::S2C::RoomCreated)) {
        m_roomId = obj.value(QLatin1String(Protocol::KeyRoomId)).toString();
        emit roomCreated(m_roomId);
    } else if (type == QLatin1String(Protocol::S2C::Joined)) {
        m_roomId = obj.value(QLatin1String(Protocol::KeyRoomId)).toString();
        m_selfId = obj.value(QLatin1String(Protocol::KeyMemberId)).toString();
        QVector<Member> members;
        const QJsonArray arr = obj.value(QLatin1String(Protocol::KeyMembers)).toArray();
        members.reserve(arr.size());
        for (const QJsonValue &v : arr)
            members.append(parseMember(v.toObject()));
        emit joined(m_roomId, m_selfId, members);
    } else if (type == QLatin1String(Protocol::S2C::MemberJoined)) {
        emit memberJoined(parseMember(obj.value(QLatin1String(Protocol::KeyMember)).toObject()));
    } else if (type == QLatin1String(Protocol::S2C::MemberLeft)) {
        emit memberLeft(obj.value(QLatin1String(Protocol::KeyMemberId)).toString());
    } else if (type == QLatin1String(Protocol::S2C::MemberUpdated)) {
        emit memberUpdated(obj.value(QLatin1String(Protocol::KeyMemberId)).toString(),
                           obj.value(QLatin1String(Protocol::KeyMic)).toBool(),
                           obj.value(QLatin1String(Protocol::KeyCam)).toBool(),
                           obj.value(QLatin1String(Protocol::KeySharing)).toBool());
    } else if (type == QLatin1String(Protocol::S2C::Chat)) {
        emit chatMessage(obj.value(QLatin1String(Protocol::KeyMemberId)).toString(),
                         obj.value(QLatin1String(Protocol::KeyName)).toString(),
                         obj.value(QLatin1String(Protocol::KeyText)).toString());
    } else if (type == QLatin1String(Protocol::S2C::RoomClosed)) {
        m_roomId.clear();
        emit roomClosed();
    } else if (type == QLatin1String(Protocol::S2C::Error)) {
        emit errorOccurred(obj.value(QLatin1String(Protocol::KeyCode)).toString(),
                           obj.value(QLatin1String(Protocol::KeyMessage)).toString());
    } else if (type == QLatin1String(Protocol::S2C::Pong)) {
        emit pong();
    }
}

void SignalingClient::sendJson(const QJsonObject &obj)
{
    if (!isConnected())
        return;
    m_socket->sendTextMessage(
        QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

SignalingClient::Member SignalingClient::parseMember(const QJsonObject &obj)
{
    Member m;
    m.id = obj.value(QLatin1String(Protocol::KeyId)).toString();
    m.name = obj.value(QLatin1String(Protocol::KeyName)).toString();
    m.micOn = obj.value(QLatin1String(Protocol::KeyMic)).toBool();
    m.camOn = obj.value(QLatin1String(Protocol::KeyCam)).toBool();
    m.sharing = obj.value(QLatin1String(Protocol::KeySharing)).toBool();
    m.isHost = obj.value(QLatin1String(Protocol::KeyIsHost)).toBool();
    return m;
}

// 媒体帧二进制信封： [4 字节大端 headerLen][header JSON][payload]
QByteArray SignalingClient::packEnvelope(const QJsonObject &header, const QByteArray &payload)
{
    const QByteArray hdr = QJsonDocument(header).toJson(QJsonDocument::Compact);
    QByteArray out;
    const quint32 n = quint32(hdr.size());
    out.append(char((n >> 24) & 0xff));
    out.append(char((n >> 16) & 0xff));
    out.append(char((n >> 8) & 0xff));
    out.append(char(n & 0xff));
    out.append(hdr);
    out.append(payload);
    return out;
}

bool SignalingClient::unpackEnvelope(const QByteArray &data, QJsonObject &meta,
                                     QByteArray &payload)
{
    if (data.size() < 4)
        return false;
    const quint32 n = (quint32(uchar(data[0])) << 24) | (quint32(uchar(data[1])) << 16)
                    | (quint32(uchar(data[2])) << 8) | quint32(uchar(data[3]));
    if (n > 65536 || data.size() < 4 + int(n))
        return false;
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data.mid(4, int(n)), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    meta = doc.object();
    payload = data.mid(4 + int(n));
    return true;
}
