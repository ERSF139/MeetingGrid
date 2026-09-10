#pragma once

#include <QObject>
#include <QString>
#include <QVector>

class QJsonObject;
class QWebSocket;

// WebSocket 信令客户端：封装与 MeetingGridServer 的协议交互。
// 协议定义见 docs/信令协议.md，消息常量复用 server/protocol.h。
// 单向职责：连接、发消息、把服务端推送解析成易用信号，不持有界面状态。
class SignalingClient : public QObject
{
    Q_OBJECT
public:
    // 一名远端/本机成员的会话快照（与服务端 member 对象字段一致）
    struct Member {
        QString id;
        QString name;
        bool    micOn   = true;
        bool    camOn   = true;
        bool    sharing = false;
        bool    isHost  = false;
    };

    explicit SignalingClient(QObject *parent = nullptr);
    ~SignalingClient() override;

    bool isConnected() const;
    QString roomId() const;
    QString selfId() const;
    bool isInRoom() const { return !m_roomId.isEmpty(); }

    // 连接 / 断开（url 形如 ws://host:port）
    void connectToServer(const QString &url);
    void disconnectFromServer();

    // 协议操作
    void createRoom(const QString &name);
    void joinRoom(const QString &roomId, const QString &name);
    void leaveRoom();
    void setState(bool mic, bool cam);
    void setShare(bool on);
    void sendChat(const QString &text);
    void ping();

    // 媒体帧（视频/音频/屏幕）经服务端中继给同会其他成员。
    // meta 需含 "kind"（video|audio|screen），服务端会注入 "member_id"。
    void sendMedia(const QJsonObject &meta, const QByteArray &payload);

signals:
    void connected();
    void disconnected();
    void roomCreated(const QString &roomId);
    void joined(const QString &roomId, const QString &selfId,
                const QVector<SignalingClient::Member> &members);
    void memberJoined(const SignalingClient::Member &member);
    void memberLeft(const QString &memberId);
    void memberUpdated(const QString &memberId, bool mic, bool cam, bool sharing);
    void chatMessage(const QString &memberId, const QString &name, const QString &text);
    void roomClosed();
    void errorOccurred(const QString &code, const QString &message);
    void pong();
    // 收到媒体帧（服务端中继）。kind: video|audio|screen
    void mediaFrameReceived(const QString &kind, const QString &senderId,
                            const QByteArray &payload, const QJsonObject &meta);

private:
    void onConnected();
    void onTextMessage(const QString &message);
    void onBinaryMessage(const QByteArray &data);
    void onDisconnected();
    void sendJson(const QJsonObject &obj);
    static Member parseMember(const QJsonObject &obj);
    static QByteArray packEnvelope(const QJsonObject &header, const QByteArray &payload);
    static bool unpackEnvelope(const QByteArray &data, QJsonObject &meta, QByteArray &payload);

    QWebSocket *m_socket = nullptr;
    QString m_roomId;
    QString m_selfId;
};

Q_DECLARE_METATYPE(SignalingClient::Member)
