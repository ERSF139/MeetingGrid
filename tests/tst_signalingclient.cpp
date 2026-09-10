#include "SignalingClient.h"
#include "SignalingServer.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QTest>
#include <QThread>
#include <QVector>

// 信令集成测试：进程内启动真实 SignalingServer，驱动两个 SignalingClient，
// 覆盖 建会 → 入会 → 状态同步 → 聊天 → 退会 → 主持人离开关房。
// 说明：用普通 connect + 计数器，绕开 QSignalSpy 在本环境下的偶发漏记问题。
class TestSignalingClient : public QObject
{
    Q_OBJECT
private slots:
    void fullFlow();
};

// 轮询事件循环直到条件成立（默认 5s 超时）
static bool waitFor(const std::function<bool()> &cond, int timeoutMs = 5000)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        if (cond())
            return true;
        QThread::msleep(5);
    }
    return cond();
}

void TestSignalingClient::fullFlow()
{
    // 进程内服务端（端口 0 = 系统分配）
    SignalingServer server(0);
    QVERIFY(server.start(QStringLiteral("127.0.0.1")));
    const QString url = QStringLiteral("ws://127.0.0.1:%1").arg(server.port());

    SignalingClient A; // 主持人
    SignalingClient B; // 成员（张伟）
    SignalingClient C; // 成员（李娜）

    // ---- 记录器（纯 connect，绕开 QSignalSpy 在本环境的漏记） ----
    int aConnected = 0, bConnected = 0, cConnected = 0;
    QString roomId;
    QString selfIdA, selfIdB;
    int aJoined = 0, bJoined = 0, cJoined = 0;
    QVector<SignalingClient::Member> membersOnJoinA, membersOnJoinB, membersOnJoinC;
    int aMemberJoined = 0;
    SignalingClient::Member lastJoinedMember;
    int aMemberLeft = 0;
    QString leftMemberId;
    int aUpdated = 0, bUpdated = 0, cUpdated = 0;
    QString updatedId;
    bool updatedMic = true, updatedCam = true;
    int aChat = 0, bChat = 0, cChat = 0;
    QString lastChatName, lastChatText;
    int cRoomClosed = 0;
    int bMediaVideo = 0, bMediaAudio = 0;
    QString mediaSender;
    QByteArray mediaPayload;

    connect(&A, &SignalingClient::connected, this, [&]() { ++aConnected; });
    connect(&B, &SignalingClient::connected, this, [&]() { ++bConnected; });
    connect(&C, &SignalingClient::connected, this, [&]() { ++cConnected; });
    connect(&A, &SignalingClient::joined, this,
            [&](const QString &r, const QString &selfId,
                const QVector<SignalingClient::Member> &members) {
                ++aJoined; roomId = r; selfIdA = selfId; membersOnJoinA = members;
            });
    connect(&B, &SignalingClient::joined, this,
            [&](const QString &r, const QString &selfId,
                const QVector<SignalingClient::Member> &members) {
                ++bJoined; selfIdB = selfId; membersOnJoinB = members; Q_UNUSED(r);
            });
    connect(&C, &SignalingClient::joined, this,
            [&](const QString &r, const QString &selfId,
                const QVector<SignalingClient::Member> &members) {
                ++cJoined; membersOnJoinC = members; Q_UNUSED(r); Q_UNUSED(selfId);
            });
    connect(&A, &SignalingClient::memberJoined, this,
            [&](const SignalingClient::Member &m) { ++aMemberJoined; lastJoinedMember = m; });
    connect(&A, &SignalingClient::memberLeft, this,
            [&](const QString &id) { ++aMemberLeft; leftMemberId = id; });
    connect(&A, &SignalingClient::memberUpdated, this,
            [&](const QString &id, bool mic, bool cam, bool) { ++aUpdated; updatedId = id; updatedMic = mic; updatedCam = cam; });
    connect(&B, &SignalingClient::memberUpdated, this, [&]() { ++bUpdated; });
    connect(&C, &SignalingClient::memberUpdated, this, [&]() { ++cUpdated; });
    connect(&A, &SignalingClient::chatMessage, this,
            [&](const QString &, const QString &name, const QString &text) { ++aChat; lastChatName = name; lastChatText = text; });
    connect(&B, &SignalingClient::chatMessage, this, [&]() { ++bChat; });
    connect(&C, &SignalingClient::chatMessage, this, [&]() { ++cChat; });
    connect(&C, &SignalingClient::roomClosed, this, [&]() { ++cRoomClosed; });
    connect(&B, &SignalingClient::mediaFrameReceived, this,
            [&](const QString &kind, const QString &senderId,
                const QByteArray &payload, const QJsonObject &) {
                if (kind == QStringLiteral("video")) ++bMediaVideo;
                else if (kind == QStringLiteral("audio")) ++bMediaAudio;
                mediaSender = senderId;
                mediaPayload = payload;
            });

    // --- 1. 三个客户端连接 ---
    A.connectToServer(url);
    B.connectToServer(url);
    C.connectToServer(url);
    QVERIFY(waitFor([&]() { return aConnected >= 1 && bConnected >= 1 && cConnected >= 1; }));

    // --- 2. A 创建房间 ---
    A.createRoom(QStringLiteral("主持人"));
    QVERIFY(waitFor([&]() { return aJoined >= 1; }));
    QVERIFY2(!roomId.isEmpty(), "收到会议号");
    QCOMPARE(roomId.size(), 9); // 9 位数字会议号
    QVERIFY2(!selfIdA.isEmpty(), "拿到本机成员 id");
    QCOMPARE(membersOnJoinA.size(), 1);
    QCOMPARE(membersOnJoinA.at(0).isHost, true); // 创建者是主持人

    // --- 3. B、C 加入房间 ---
    B.joinRoom(roomId, QStringLiteral("张伟"));
    QVERIFY(waitFor([&]() { return bJoined >= 1 && aMemberJoined >= 1; }));
    QCOMPARE(membersOnJoinB.size(), 2); // B 的全量列表含 2 人
    QCOMPARE(lastJoinedMember.name, QStringLiteral("张伟"));
    QCOMPARE(lastJoinedMember.isHost, false);
    QVERIFY2(!selfIdB.isEmpty(), "B 拿到本机成员 id");
    QCOMPARE(selfIdB, lastJoinedMember.id); // 双方对 B 的成员 id 一致

    C.joinRoom(roomId, QStringLiteral("李娜"));
    QVERIFY(waitFor([&]() { return cJoined >= 1 && aMemberJoined >= 2; }));
    QCOMPARE(membersOnJoinC.size(), 3); // C 的全量列表含 3 人

    // --- 3.5 媒体帧中继：A 推视频/音频 → B 收到（payload 逐字节透传） ---
    const QByteArray videoPayload("VIDEO-FRAME-001");
    const QByteArray audioPayload("AUDIO-CHUNK-002");
    A.sendMedia(QJsonObject{{QStringLiteral("kind"), QStringLiteral("video")},
                            {QStringLiteral("w"), 64}, {QStringLiteral("h"), 64}},
                videoPayload);
    QVERIFY(waitFor([&]() { return bMediaVideo >= 1; }));
    QCOMPARE(mediaSender, selfIdA); // 服务端注入的发送者 id
    QCOMPARE(mediaPayload, videoPayload);

    A.sendMedia(QJsonObject{{QStringLiteral("kind"), QStringLiteral("audio")}}, audioPayload);
    QVERIFY(waitFor([&]() { return bMediaAudio >= 1; }));
    QCOMPARE(mediaPayload, audioPayload);

    // --- 4. A 关麦克风：全员收到状态广播 ---
    A.setState(false, true);
    QVERIFY(waitFor([&]() { return aUpdated >= 1 && bUpdated >= 1 && cUpdated >= 1; }));
    QCOMPARE(updatedId, selfIdA);
    QCOMPARE(updatedMic, false); // mic
    QCOMPARE(updatedCam, true);  // cam

    // --- 5. B 发聊天：全员收到（含发送者名字） ---
    B.sendChat(QStringLiteral("大家好"));
    QVERIFY(waitFor([&]() { return aChat >= 1 && bChat >= 1 && cChat >= 1; }));
    QCOMPARE(lastChatName, QStringLiteral("张伟"));
    QCOMPARE(lastChatText, QStringLiteral("大家好"));

    // --- 6. B 主动离开：A、C 收到 member_left ---
    B.leaveRoom();
    QVERIFY(waitFor([&]() { return aMemberLeft >= 1; }));
    QCOMPARE(leftMemberId, selfIdB); // B 离开，广播其成员 id

    // --- 7. 主持人 A 断开 → 仍在房内的 C 收到 room_closed ---
    A.disconnectFromServer();
    QVERIFY(waitFor([&]() { return cRoomClosed >= 1; }));
}

QTEST_MAIN(TestSignalingClient)
#include "tst_signalingclient.moc"
