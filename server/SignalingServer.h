#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

class QJsonObject;
class QTimer;
class QWebSocket;
class QWebSocketServer;

// 轻量级 WebSocket 信令服务端：
// 房间（建会/入会/退会）+ 成员状态同步（麦/摄像/共享）+ 聊天中继 + 心跳清理。
// 单线程 Qt 事件循环即可支撑信令量；协议见 docs/信令协议.md。
class SignalingServer : public QObject
{
    Q_OBJECT
public:
    // port: 监听端口（0 表示由系统分配）
    explicit SignalingServer(quint16 port, QObject *parent = nullptr);
    ~SignalingServer() override;

    // 返回是否成功开始监听；成功后用 port() 取实际端口
    bool start(const QString &host = QStringLiteral("0.0.0.0"));
    quint16 port() const;

private:
    // 一名连接成员的会话信息
    struct Member {
        QString id;
        QString name;
        QString roomId;           // 空 = 尚未入会
        bool    micOn   = true;
        bool    camOn   = true;
        bool    sharing = false;
        bool    isHost  = false;
        qint64  lastActive = 0;   // 最后一次收到消息的时刻（epoch 毫秒）
    };

    // 一个会议房间
    struct Room {
        QString           id;
        QString           hostId;      // 主持人成员 id（创建者）
        QVector<QString>  memberIds;   // 有序成员 id，保持加入顺序
    };

    void onNewConnection();
    void onTextMessage(const QString &message);
    void onBinaryMessage(const QByteArray &data);
    void onDisconnected();

    void handleCreateRoom(QWebSocket *client, const QJsonObject &obj);
    void handleJoinRoom(QWebSocket *client, const QJsonObject &obj);
    void handleLeaveRoom(QWebSocket *client);
    void handleSetState(QWebSocket *client, const QJsonObject &obj);
    void handleSetShare(QWebSocket *client, const QJsonObject &obj);
    void handleChat(QWebSocket *client, const QJsonObject &obj);
    void handlePing(QWebSocket *client);

    void heartbeatCheck();
    void removeMemberFromRoom(QWebSocket *client, bool notify);
    void closeRoom(const QString &roomId);

    void sendTo(QWebSocket *client, const QJsonObject &obj);
    void broadcastToRoom(const QString &roomId, const QJsonObject &obj,
                         QWebSocket *except = nullptr);
    void broadcastBinaryToRoom(const QString &roomId, const QByteArray &envelope,
                               QWebSocket *except);

    Member *memberFor(QWebSocket *client);
    QString makeRoomId() const;
    QJsonObject memberToJson(const Member &m) const;
    QVector<QJsonObject> roomMembersJson(const QString &roomId);

    QWebSocketServer *m_server = nullptr;
    QHash<QWebSocket *, Member> m_members;          // 连接 → 成员信息
    QHash<QString, QWebSocket *> m_socketOfMember;  // 成员 id → 连接
    QHash<QString, Room> m_rooms;                   // 房间号 → 房间
    QTimer *m_heartbeatTimer = nullptr;
    int     m_nextMemberId = 1;
    quint16 m_port = 0;

    static constexpr qint64 kIdleTimeoutMs = 120000; // 120s 无消息视为掉线
};
