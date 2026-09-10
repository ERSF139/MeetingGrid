#pragma once

#include "GridContainer.h"
#include "SignalingClient.h"
#include "VideoTile.h"

#include <QColor>
#include <QMainWindow>
#include <QPoint>
#include <QSet>
#include <QStringList>
#include <QVector>

class QAudioSink;
class QCameraDevice;
class QEvent;
class QIODevice;
class QJsonObject;
class QLabel;
class QListWidget;
class QMenu;
class QMouseEvent;
class QSlider;
class QLineEdit;
class QTimer;
class ControlButton;
class VideoSource;
class WindowButton;
class VideoSource;
class SimulatedVideoSource;
class CameraVideoSource;
class AudioLevelSource;

// 会议多宫格显示原型主窗口。
// 布局：顶部工具栏 + 中央宫格区/成员面板 + 底部控制栏。
// 核心演示：右侧面板调整成员数量 -> 宫格自动重排。
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    // initialMembers > 0 时，启动即加载指定数量的成员（用于演示各规模宫格）
    explicit MainWindow(int initialMembers = -1, QWidget *parent = nullptr);

protected:
    // 无边框窗口：顶栏拖拽 / 双击最大化 / 右键菜单
    bool eventFilter(QObject *obj, QEvent *event) override;
    // 窗口最大化/还原时刷新控制按钮图标
    void changeEvent(QEvent *event) override;
    // Windows：WM_NCHITTEST 实现边缘缩放
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    void addMember();
    void removeMember();
    void onCountSliderChanged(int value);
    void onTileClicked(int index);
    void onEndCall();
    void onMicToggle(bool on);
    void onCamToggle(bool on);
    void onShareToggle(bool on);
    void onViewToggle();
    void onChatToggle();
    void onSendChat();
    void updateTimer();
    void openSettings();
    void onSelfSpeakingChanged(bool speaking);

    // 无边框窗口控制按钮
    void onWindowMinimize();
    void onWindowMaximizeRestore();
    void onWindowClose();

    // 联网会议（信令）
    void onNetworkClicked();
    void onNetConnected();
    void onNetDisconnected();
    void onNetError(const QString &code, const QString &message);
    void onRoomCreated(const QString &roomId);
    void onJoined(const QString &roomId, const QString &selfId,
                  const QVector<SignalingClient::Member> &members);
    void onMemberJoined(const SignalingClient::Member &member);
    void onMemberLeft(const QString &memberId);
    void onMemberUpdated(const QString &memberId, bool mic, bool cam, bool sharing);
    void onChatMessage(const QString &memberId, const QString &name, const QString &text);
    void onRoomClosed();

    // 媒体传输
    void onSelfFrameForNetwork(const QImage &frame);
    void onSelfAudioData(const QByteArray &pcm);
    void onMediaFrame(const QString &kind, const QString &senderId,
                      const QByteArray &payload, const QJsonObject &meta);
    void grabAndSendScreen();

private:
    struct MemberData {
        QString name;
        QColor  color;
        bool    micOn    = true;
        bool    camOn    = true;
        bool    speaking = false;
        bool    isSelf   = false;
        bool    sharing  = false;
        bool    isHost   = false;
        QString id;       // 联网模式下的服务器成员 id（本地演示成员为空）
    };

    void buildUi();
    void buildTopBar();
    void buildGridArea();
    void buildRightPanel();
    void buildBottomBar();
    void buildChatPanel();

    // 无边框窗口：顶栏右键系统菜单
    void showTopBarMenu(const QPoint &globalPos);
    void updateWindowStateButtons();

    // 联网会议辅助
    void promptNetworkConnection();
    void applyPendingAction();
    void setOnlineMode(bool online);
    void populateFromNetwork(const QVector<SignalingClient::Member> &list, const QString &selfId);
    void resetLocalDemoMembers();
    void leaveNetworkMeeting();
    void updateMeetingIdLabel();
    QColor colorForNetworkId(const QString &id);

    // 媒体传输辅助
    void startScreenSharing();
    void stopScreenSharing();
    void setupRemoteAudio();
    void teardownRemoteAudio();
    void writeRemoteAudio(const QByteArray &pcm);
    int memberIndexById(const QString &id) const;

    void rebuildTiles();
    void updateMemberList();
    void updateCountLabel();
    void updateSelfTileState();
    void updateShareUi();
    void updateViewButton();
    void updateTimerLabel();
    void applySpeaking();

    MemberData nextMember();
    VideoTile *makeTile(const MemberData &m, int index);
    void setMemberCount(int n);

    // 视频源管理
    void releaseTransientSources();
    VideoSource *ensureSelfSource();
    void recreateSelfCamera(const QCameraDevice &device);
    void onCameraError(const QString &message);
    void applyViewSettings();

    // 自动权限处理
    void requestAutoPermissions();
    void upgradeSelfToCamera();
    void createCameraSelfSource(const QCameraDevice &device);

    // 音频采集（真实麦克风 + 说话检测）
    void startAudioCapture();
    void stopAudioCapture();

    // 顶栏
    QLabel *m_titleLabel   = nullptr;
    QLabel *m_meetingIdLabel = nullptr;
    QLabel *m_timerLabel   = nullptr;
    QLabel *m_countBadge   = nullptr;

    // 无边框窗口：顶栏兼作拖拽区 + 自绘窗口控制按钮
    QWidget      *m_topBar   = nullptr;
    WindowButton *m_minBtn   = nullptr;
    WindowButton *m_maxBtn   = nullptr;
    WindowButton *m_closeBtn = nullptr;
    bool         m_dragging = false;
    QPoint       m_dragOffset;

    // 宫格 + 面板
    GridContainer *m_grid = nullptr;
    QWidget      *m_rightPanel = nullptr;
    QListWidget  *m_memberList = nullptr;
    QSlider      *m_countSlider = nullptr;
    QLabel       *m_sliderValue = nullptr;
    QLabel       *m_gridHint = nullptr;

    // 底部控制栏
    ControlButton *m_micBtn   = nullptr;
    ControlButton *m_camBtn   = nullptr;
    ControlButton *m_shareBtn = nullptr;
    ControlButton *m_chatBtn  = nullptr;
    ControlButton *m_networkBtn = nullptr;
    ControlButton *m_viewBtn  = nullptr;
    ControlButton *m_settingsBtn = nullptr;
    ControlButton *m_endBtn   = nullptr;

    // 联网会议（信令）
    SignalingClient *m_netClient = nullptr;
    bool m_online = false;                       // 是否处于联网模式
    QString m_netServerUrl;
    QString m_pendingRoomId;                     // 空 = 创建新会议
    QString m_pendingName;
    QHash<QString, QColor> m_netColorOf;         // 远端成员 id → 稳定颜色
    QWidget *m_demoSection = nullptr;            // 本地演示控制区（联网时隐藏）
    QLabel  *m_demoNote = nullptr;

    // 媒体传输（WebSocket 帧中继）
    QTimer   *m_shareTimer = nullptr;            // 屏幕共享抓帧定时器
    bool      m_sharingScreen = false;           // 是否正在共享屏幕
    bool      m_shareAutoPausedCam = false;      // 共享时是否已自动暂停摄像头
    qint64    m_lastSelfVideoMs = 0;             // 自视频推流节流时间戳
    QAudioSink *m_remoteAudioSink = nullptr;     // 远端音频播放
    QIODevice *m_remoteAudioSinkDev = nullptr;
    QVector<VideoSource *> m_remoteSources;      // 远端成员模拟源（按 m_members 索引，self 为 nullptr）
    QSet<QString> m_realVideoMembers;            // 已收到真实画面的成员 id（重建时不再启动模拟源）

    // 聊天面板
    QWidget      *m_chatPanel = nullptr;
    QListWidget  *m_chatList = nullptr;
    QLineEdit    *m_chatEdit = nullptr;

    // 状态
    QTimer *m_timer = nullptr;
    int     m_elapsed = 0;
    bool    m_sharing = false;

    // 视频源
    VideoSource *m_selfSource = nullptr;
    QVector<VideoSource *> m_transientSources;
    VideoTile::FillMode m_fillMode = VideoTile::FillMode::Crop;
    int  m_simulatedFps = 15;
    QString m_selectedCameraId;

    // 音频源
    AudioLevelSource *m_audioSource = nullptr;

    QVector<MemberData> m_members;
    int m_selectedIndex = -1;

    // 名字 / 颜色池
    QStringList m_namePool;
    QVector<QColor> m_colorPool;
    int m_nameCursor  = 0;
    int m_colorCursor = 0;
};
