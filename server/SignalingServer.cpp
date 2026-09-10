#include "SignalingServer.h"
#include "protocol.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>

namespace {
// 带时间戳的控制台日志（UTF-8）
void logMsg(const QString &text)
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    qInfo().noquote() << QStringLiteral("[%1] %2").arg(stamp, text);
}

QString sanitizeName(const QString &raw)
{
    QString n = raw.trimmed();
    if (n.isEmpty())
        n = QStringLiteral("成员");
    return n.left(32); // 限制名字长度
}

// 规范化房间号：去掉空格 / 短横线 / 括号，便于粘贴 "882 536 419" 这类展示格式
QString normalizeRoomId(const QString &raw)
{
    QString out;
    out.reserve(raw.size());
    for (const QChar &c : raw) {
        if (c.isDigit())
            out.append(c);
    }
    return out;
}

QJsonArray toJsonArray(const QVector<QJsonObject> &list)
{
    QJsonArray arr;
    for (const QJsonObject &o : list)
        arr.append(o);
    return arr;
}

// 媒体帧二进制信封： [4 字节大端 headerLen][header JSON][payload]
QByteArray packEnvelope(const QJsonObject &header, const QByteArray &payload)
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

// 解析信封；成功返回 true 并把 header 放入 meta
bool unpackEnvelope(const QByteArray &data, QJsonObject &meta, QByteArray &payload)
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
} // namespace

SignalingServer::SignalingServer(quint16 port, QObject *parent)
    : QObject(parent)
    , m_port(port)
{
    m_server = new QWebSocketServer(QStringLiteral("MeetingGridSignaling"),
                                    QWebSocketServer::NonSecureMode, this);
    connect(m_server, &QWebSocketServer::newConnection, this, &SignalingServer::onNewConnection);
    connect(m_server, &QWebSocketServer::serverError, this, [this](QWebSocketProtocol::CloseCode) {
        logMsg(QStringLiteral("监听错误: %1").arg(m_server->errorString()));
    });

    // 心跳：周期扫描空闲连接，超时主动断开
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(15000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &SignalingServer::heartbeatCheck);
    m_heartbeatTimer->start();
}

SignalingServer::~SignalingServer()
{
    if (m_server)
        m_server->close();
}

bool SignalingServer::start(const QString &host)
{
    if (!m_server->listen(QHostAddress(host), m_port)) {
        qWarning().noquote() << QStringLiteral("无法监听 %1:%2 - %3")
                                    .arg(host)
                                    .arg(m_port)
                                    .arg(m_server->errorString());
        return false;
    }
    m_port = m_server->serverPort();
    logMsg(QStringLiteral("信令服务已启动，监听 %1:%2").arg(host).arg(m_port));
    return true;
}

quint16 SignalingServer::port() const
{
    return m_port;
}

// ---------------------------------------------------------------------------
// 连接生命周期
// ---------------------------------------------------------------------------

void SignalingServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QWebSocket *sock = m_server->nextPendingConnection();
        if (!sock)
            continue;

        Member m;
        m.id = QStringLiteral("m%1").arg(m_nextMemberId++);
        m.lastActive = QDateTime::currentMSecsSinceEpoch();
        m_members.insert(sock, m);
        m_socketOfMember.insert(m.id, sock);

        // 媒体帧为缩放后 JPEG（≤1280px），默认 1MB 消息上限已足够
        connect(sock, &QWebSocket::textMessageReceived, this, &SignalingServer::onTextMessage);
        connect(sock, &QWebSocket::binaryMessageReceived, this, &SignalingServer::onBinaryMessage);
        connect(sock, &QWebSocket::disconnected, this, &SignalingServer::onDisconnected);
        connect(sock, &QWebSocket::errorOccurred, this, [sock](QAbstractSocket::SocketError) {
            logMsg(QStringLiteral("客户端 %1 出错: %2")
                       .arg(sock->peerAddress().toString(), sock->errorString()));
        });

        QJsonObject hello;
        hello[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::Hello);
        hello[QLatin1String(Protocol::KeyVersion)] = QLatin1String(Protocol::ServerVersion);
        sendTo(sock, hello);

        logMsg(QStringLiteral("客户端接入: %1 (%2)")
                   .arg(m.id, sock->peerAddress().toString()));
    }
}

void SignalingServer::onTextMessage(const QString &message)
{
    auto *client = qobject_cast<QWebSocket *>(sender());
    if (!client)
        return;

    Member *m = memberFor(client);
    if (m)
        m->lastActive = QDateTime::currentMSecsSinceEpoch();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        logMsg(QStringLiteral("收到非法 JSON，已忽略: %1").arg(message.left(120)));
        return;
    }
    const QJsonObject obj = doc.object();
    const QString type = obj.value(QLatin1String(Protocol::KeyType)).toString();

    if (type == QLatin1String(Protocol::C2S::CreateRoom))
        handleCreateRoom(client, obj);
    else if (type == QLatin1String(Protocol::C2S::JoinRoom))
        handleJoinRoom(client, obj);
    else if (type == QLatin1String(Protocol::C2S::LeaveRoom))
        handleLeaveRoom(client);
    else if (type == QLatin1String(Protocol::C2S::SetState))
        handleSetState(client, obj);
    else if (type == QLatin1String(Protocol::C2S::SetShare))
        handleSetShare(client, obj);
    else if (type == QLatin1String(Protocol::C2S::Chat))
        handleChat(client, obj);
    else if (type == QLatin1String(Protocol::C2S::Ping))
        handlePing(client);
    else
        logMsg(QStringLiteral("未知消息类型: %1").arg(type));
}

void SignalingServer::onBinaryMessage(const QByteArray &data)
{
    auto *client = qobject_cast<QWebSocket *>(sender());
    Member *m = memberFor(client);
    if (!m || m->roomId.isEmpty())
        return; // 不在房内不转发媒体

    QJsonObject header;
    QByteArray payload;
    if (!unpackEnvelope(data, header, payload))
        return;

    // 注入权威发送者 id，再透传给同会其他成员
    header[QLatin1String(Protocol::KeyMemberId)] = m->id;
    broadcastBinaryToRoom(m->roomId, packEnvelope(header, payload), client);
}

void SignalingServer::onDisconnected()
{
    auto *client = qobject_cast<QWebSocket *>(sender());
    if (!client)
        return;

    Member *m = memberFor(client);
    const QString id = m ? m->id : QStringLiteral("?");

    // 若在房间内，先广播离开/关房
    if (m && !m->roomId.isEmpty())
        removeMemberFromRoom(client, /*notify=*/true);
    else
        logMsg(QStringLiteral("客户端断开: %1").arg(id));

    m_members.remove(client);
    m_socketOfMember.remove(id);
    client->deleteLater();
}

// ---------------------------------------------------------------------------
// 协议处理
// ---------------------------------------------------------------------------

void SignalingServer::handleCreateRoom(QWebSocket *client, const QJsonObject &obj)
{
    Member *m = memberFor(client);
    if (!m)
        return;
    if (!m->roomId.isEmpty()) {
        QJsonObject e;
        e[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::Error);
        e[QLatin1String(Protocol::KeyCode)] = QLatin1String(Protocol::ErrInRoom);
        e[QLatin1String(Protocol::KeyMessage)] = QStringLiteral("你已在会议中，请先离开");
        sendTo(client, e);
        return;
    }

    m->name = sanitizeName(obj.value(QLatin1String(Protocol::KeyName)).toString());
    m->isHost = true;
    m->micOn = true;
    m->camOn = true;
    m->sharing = false;

    const QString roomId = makeRoomId();
    m->roomId = roomId;

    Room room;
    room.id = roomId;
    room.hostId = m->id;
    room.memberIds.append(m->id);
    m_rooms.insert(roomId, room);

    QJsonObject created;
    created[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::RoomCreated);
    created[QLatin1String(Protocol::KeyRoomId)] = roomId;
    sendTo(client, created);

    QJsonObject joined;
    joined[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::Joined);
    joined[QLatin1String(Protocol::KeyRoomId)] = roomId;
    joined[QLatin1String(Protocol::KeyMemberId)] = m->id;
    joined.insert(QLatin1String(Protocol::KeyMembers), toJsonArray(roomMembersJson(roomId)));
    sendTo(client, joined);

    logMsg(QStringLiteral("[房间 %1] %2(%3) 创建会议，成为主持人")
               .arg(roomId, m->name, m->id));
}

void SignalingServer::handleJoinRoom(QWebSocket *client, const QJsonObject &obj)
{
    Member *m = memberFor(client);
    if (!m)
        return;
    if (!m->roomId.isEmpty()) {
        QJsonObject e;
        e[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::Error);
        e[QLatin1String(Protocol::KeyCode)] = QLatin1String(Protocol::ErrInRoom);
        e[QLatin1String(Protocol::KeyMessage)] = QStringLiteral("你已在会议中，请先离开");
        sendTo(client, e);
        return;
    }

    const QString roomId = normalizeRoomId(
        obj.value(QLatin1String(Protocol::KeyRoomId)).toString());
    auto it = m_rooms.find(roomId);
    if (it == m_rooms.end()) {
        QJsonObject e;
        e[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::Error);
        e[QLatin1String(Protocol::KeyCode)] = QLatin1String(Protocol::ErrRoomNotFound);
        e[QLatin1String(Protocol::KeyMessage)] = QStringLiteral("会议号不存在：%1").arg(roomId);
        sendTo(client, e);
        return;
    }
    Room &room = it.value();
    if (room.memberIds.size() >= Protocol::MaxRoomMembers) {
        QJsonObject e;
        e[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::Error);
        e[QLatin1String(Protocol::KeyCode)] = QLatin1String(Protocol::ErrRoomFull);
        e[QLatin1String(Protocol::KeyMessage)] = QStringLiteral("会议已满员（%1 人）").arg(Protocol::MaxRoomMembers);
        sendTo(client, e);
        return;
    }

    m->name = sanitizeName(obj.value(QLatin1String(Protocol::KeyName)).toString());
    m->roomId = roomId;
    m->isHost = false;
    m->micOn = true;
    m->camOn = true;
    m->sharing = false;
    room.memberIds.append(m->id);

    QJsonObject joined;
    joined[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::Joined);
    joined[QLatin1String(Protocol::KeyRoomId)] = roomId;
    joined[QLatin1String(Protocol::KeyMemberId)] = m->id;
    joined.insert(QLatin1String(Protocol::KeyMembers), toJsonArray(roomMembersJson(roomId)));
    sendTo(client, joined);

    QJsonObject joinedMsg;
    joinedMsg[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::MemberJoined);
    joinedMsg[QLatin1String(Protocol::KeyMember)] = memberToJson(*m);
    broadcastToRoom(roomId, joinedMsg, client);

    logMsg(QStringLiteral("[房间 %1] %2(%3) 加入（当前 %4 人）")
               .arg(roomId, m->name, m->id)
               .arg(room.memberIds.size()));
}

void SignalingServer::handleLeaveRoom(QWebSocket *client)
{
    Member *m = memberFor(client);
    if (!m || m->roomId.isEmpty()) {
        logMsg(QStringLiteral("收到 leave_room，但该客户端不在会议中"));
        return;
    }
    logMsg(QStringLiteral("[房间 %1] %2(%3) 主动离开")
               .arg(m->roomId, m->name, m->id));
    removeMemberFromRoom(client, /*notify=*/true);
}

void SignalingServer::handleSetState(QWebSocket *client, const QJsonObject &obj)
{
    Member *m = memberFor(client);
    if (!m || m->roomId.isEmpty())
        return;

    bool changed = false;
    if (obj.contains(QLatin1String(Protocol::KeyMic)) && obj.value(QLatin1String(Protocol::KeyMic)).isBool()) {
        m->micOn = obj.value(QLatin1String(Protocol::KeyMic)).toBool();
        changed = true;
    }
    if (obj.contains(QLatin1String(Protocol::KeyCam)) && obj.value(QLatin1String(Protocol::KeyCam)).isBool()) {
        m->camOn = obj.value(QLatin1String(Protocol::KeyCam)).toBool();
        changed = true;
    }
    if (!changed)
        return;

    QJsonObject upd;
    upd[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::MemberUpdated);
    upd[QLatin1String(Protocol::KeyMemberId)] = m->id;
    upd[QLatin1String(Protocol::KeyMic)] = m->micOn;
    upd[QLatin1String(Protocol::KeyCam)] = m->camOn;
    upd[QLatin1String(Protocol::KeySharing)] = m->sharing;
    broadcastToRoom(m->roomId, upd);
    logMsg(QStringLiteral("[房间 %1] %2 状态更新 mic=%3 cam=%4")
               .arg(m->roomId, m->name).arg(m->micOn).arg(m->camOn));
}

void SignalingServer::handleSetShare(QWebSocket *client, const QJsonObject &obj)
{
    Member *m = memberFor(client);
    if (!m || m->roomId.isEmpty())
        return;

    m->sharing = obj.value(QLatin1String(Protocol::KeyOn)).toBool();

    QJsonObject upd;
    upd[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::MemberUpdated);
    upd[QLatin1String(Protocol::KeyMemberId)] = m->id;
    upd[QLatin1String(Protocol::KeyMic)] = m->micOn;
    upd[QLatin1String(Protocol::KeyCam)] = m->camOn;
    upd[QLatin1String(Protocol::KeySharing)] = m->sharing;
    broadcastToRoom(m->roomId, upd);
    logMsg(QStringLiteral("[房间 %1] %2 %3 共享")
               .arg(m->roomId, m->name, m->sharing ? QStringLiteral("开始") : QStringLiteral("停止")));
}

void SignalingServer::handleChat(QWebSocket *client, const QJsonObject &obj)
{
    Member *m = memberFor(client);
    if (!m || m->roomId.isEmpty())
        return;

    const QString text = obj.value(QLatin1String(Protocol::KeyText)).toString().trimmed();
    if (text.isEmpty())
        return;

    QJsonObject chat;
    chat[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::Chat);
    chat[QLatin1String(Protocol::KeyMemberId)] = m->id;
    chat[QLatin1String(Protocol::KeyName)] = m->name;
    chat[QLatin1String(Protocol::KeyText)] = text.left(500);
    broadcastToRoom(m->roomId, chat);
    logMsg(QStringLiteral("[房间 %1] %2: %3").arg(m->roomId, m->name, text.left(80)));
}

void SignalingServer::handlePing(QWebSocket *client)
{
    QJsonObject pong;
    pong[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::Pong);
    sendTo(client, pong);
}

// ---------------------------------------------------------------------------
// 房间 / 成员维护
// ---------------------------------------------------------------------------

void SignalingServer::removeMemberFromRoom(QWebSocket *client, bool notify)
{
    Member *m = memberFor(client);
    if (!m || m->roomId.isEmpty())
        return;

    const QString roomId = m->roomId;
    auto it = m_rooms.find(roomId);
    if (it == m_rooms.end()) {
        m->roomId.clear();
        m_socketOfMember.remove(m->id);
        return;
    }
    Room &room = it.value();
    const QString leavingId = m->id;
    const bool hostLeaving = m->isHost;

    room.memberIds.removeAll(leavingId);
    m->roomId.clear();
    m_socketOfMember.remove(leavingId);

    if (notify) {
        QJsonObject left;
        left[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::MemberLeft);
        left[QLatin1String(Protocol::KeyMemberId)] = leavingId;
        broadcastToRoom(roomId, left, client);
    }

    if (hostLeaving) {
        closeRoom(roomId);
    } else if (room.memberIds.isEmpty()) {
        logMsg(QStringLiteral("[房间 %1] 已空，销毁").arg(roomId));
        m_rooms.erase(it);
    }
}

void SignalingServer::closeRoom(const QString &roomId)
{
    auto it = m_rooms.find(roomId);
    if (it == m_rooms.end())
        return;
    const Room room = it.value();
    m_rooms.erase(it);

    // 通知剩余成员房间已关闭，并清除其房间归属
    QJsonObject closed;
    closed[QLatin1String(Protocol::KeyType)] = QLatin1String(Protocol::S2C::RoomClosed);
    for (const QString &mid : room.memberIds) {
        QWebSocket *sock = m_socketOfMember.value(mid);
        if (Member *m = memberFor(sock)) {
            m->roomId.clear();
            m->isHost = false;
            sendTo(sock, closed);
        }
    }
    logMsg(QStringLiteral("[房间 %1] 已关闭（主持人离开）").arg(roomId));
}

void SignalingServer::heartbeatCheck()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_members.begin(); it != m_members.end(); ++it) {
        if (now - it.value().lastActive > kIdleTimeoutMs) {
            logMsg(QStringLiteral("%1 心跳超时，断开连接").arg(it.value().id));
            it.key()->close(); // 触发 disconnected，由 onDisconnected 完成清理
        }
    }
}

// ---------------------------------------------------------------------------
// 发送 / 工具
// ---------------------------------------------------------------------------

void SignalingServer::sendTo(QWebSocket *client, const QJsonObject &obj)
{
    if (!client || client->state() != QAbstractSocket::ConnectedState)
        return;
    client->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void SignalingServer::broadcastToRoom(const QString &roomId, const QJsonObject &obj,
                                      QWebSocket *except)
{
    auto it = m_rooms.find(roomId);
    if (it == m_rooms.end())
        return;
    for (const QString &mid : it.value().memberIds) {
        QWebSocket *sock = m_socketOfMember.value(mid);
        if (sock && sock != except)
            sendTo(sock, obj);
    }
}

void SignalingServer::broadcastBinaryToRoom(const QString &roomId, const QByteArray &envelope,
                                            QWebSocket *except)
{
    auto it = m_rooms.find(roomId);
    if (it == m_rooms.end())
        return;
    for (const QString &mid : it.value().memberIds) {
        QWebSocket *sock = m_socketOfMember.value(mid);
        if (sock && sock != except && sock->state() == QAbstractSocket::ConnectedState)
            sock->sendBinaryMessage(envelope);
    }
}

SignalingServer::Member *SignalingServer::memberFor(QWebSocket *client)
{
    if (!client)
        return nullptr;
    auto it = m_members.find(client);
    return it == m_members.end() ? nullptr : &it.value();
}

QString SignalingServer::makeRoomId() const
{
    QString id;
    do {
        const quint32 n = QRandomGenerator::global()->bounded(1000000000u);
        id = QStringLiteral("%1").arg(n, Protocol::RoomIdDigits, 10, QLatin1Char('0'));
    } while (m_rooms.contains(id));
    return id;
}

QJsonObject SignalingServer::memberToJson(const Member &m) const
{
    QJsonObject o;
    o[QLatin1String(Protocol::KeyId)] = m.id;   // member 对象内用 "id"
    o[QLatin1String(Protocol::KeyName)] = m.name;
    o[QLatin1String(Protocol::KeyMic)] = m.micOn;
    o[QLatin1String(Protocol::KeyCam)] = m.camOn;
    o[QLatin1String(Protocol::KeySharing)] = m.sharing;
    o[QLatin1String(Protocol::KeyIsHost)] = m.isHost;
    return o;
}

QVector<QJsonObject> SignalingServer::roomMembersJson(const QString &roomId)
{
    QVector<QJsonObject> out;
    auto it = m_rooms.find(roomId);
    if (it == m_rooms.end())
        return out;
    for (const QString &mid : it.value().memberIds) {
        QWebSocket *sock = m_socketOfMember.value(mid);
        if (const Member *m = memberFor(sock))
            out.append(memberToJson(*m));
    }
    return out;
}
