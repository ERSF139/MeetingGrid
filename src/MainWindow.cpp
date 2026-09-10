#include "MainWindow.h"

#include "AudioSource.h"
#include "ControlButton.h"
#include "IconFactory.h"
#include "VideoSource.h"
#include "VideoTile.h"
#include "protocol.h"

#include <QApplication>
#include <QAudioDevice>
#include <QAudioSink>
#include <QBuffer>
#include <QCameraDevice>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QEvent>
#include <QGuiApplication>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>
#include <QPermissions> // QCameraPermission / QMicrophonePermission (Qt 6.10 起位于 QtCore)
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScreen>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
const char *kMeetingId = "882 536 419";

// QImage 编码为 JPEG 字节（网络推流用）
QByteArray encodeJpeg(const QImage &img, int quality)
{
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "JPG", quality);
    return out;
}

// 摄像头权限是否已授予（Qt 6.5+；旧版本视为已授予，由 QCamera 自身处理）
bool cameraPermissionGranted()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QCoreApplication::instance()->checkPermission(QCameraPermission())
           == Qt::PermissionStatus::Granted;
#else
    return true;
#endif
}
} // namespace

// 无边框窗口标题栏控制按钮（最小化/最大化/关闭）：悬停高亮，居中手绘图标
// 定义为全局类以便 MainWindow 头文件前向声明、运行期切换最大化/还原图标。
class WindowButton final : public QPushButton
{
public:
    explicit WindowButton(IconFactory::IconKind icon, QWidget *parent = nullptr)
        : QPushButton(parent)
        , m_icon(icon)
    {
        setFixedSize(38, 32);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
    }

    void setIconKind(IconFactory::IconKind icon)
    {
        m_icon = icon;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const bool active = underMouse() || isDown();
        if (m_icon == IconFactory::IconKind::Close) {
            // 关闭按钮悬停为红色
            if (active) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0xe8, 0x40, 0x35));
                p.drawRect(rect());
            }
        } else if (active) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 14));
            p.drawRect(rect());
        }

        const QColor color = (m_icon == IconFactory::IconKind::Close && active)
                                 ? QColor(Qt::white)
                                 : QColor(0x40, 0x46, 0x4e);
        const qreal s = 14;
        IconFactory::drawIcon(p, QRectF((width() - s) / 2.0, (height() - s) / 2.0, s, s),
                              m_icon, color);
    }

private:
    IconFactory::IconKind m_icon;
};

MainWindow::MainWindow(int initialMembers, QWidget *parent)
    : QMainWindow(parent)
{
    // 无边框窗口：去掉系统标题栏，顶栏兼作拖拽区 + 自绘最小化/最大化/关闭
    setWindowFlag(Qt::FramelessWindowHint, true);

    // 头像配色池
    m_colorPool = {
        QColor(0x5b, 0x8d, 0xef), // 蓝
        QColor(0x2e, 0xb8, 0x8b), // 绿
        QColor(0xe0, 0x7a, 0x3a), // 橙
        QColor(0x9b, 0x6b, 0xd8), // 紫
        QColor(0xec, 0x6b, 0x8b), // 粉
        QColor(0x3a, 0xb6, 0xd8), // 青
        QColor(0xc7, 0x8b, 0x3a), // 金黄
        QColor(0x6a, 0xb0, 0x5a), // 草绿
        QColor(0x8d, 0x6b, 0x8d), // 酒红
        QColor(0x4a, 0x90, 0xa8), // 深青
    };

    // 成员名字池
    m_namePool = {
        QStringLiteral("张伟"), QStringLiteral("王芳"), QStringLiteral("李娜"),
        QStringLiteral("刘洋"), QStringLiteral("陈静"), QStringLiteral("杨勇"),
        QStringLiteral("赵敏"), QStringLiteral("黄磊"), QStringLiteral("周杰"),
        QStringLiteral("吴倩"), QStringLiteral("徐明"), QStringLiteral("孙丽"),
        QStringLiteral("马超"), QStringLiteral("朱婷"), QStringLiteral("胡军"),
        QStringLiteral("郭靖"), QStringLiteral("林峰"), QStringLiteral("何冰"),
        QStringLiteral("高远"), QStringLiteral("罗阳"), QStringLiteral("郑爽"),
        QStringLiteral("梁博"), QStringLiteral("谢娜"), QStringLiteral("唐宇"),
        QStringLiteral("韩梅"), QStringLiteral("冯雪"), QStringLiteral("董强"),
        QStringLiteral("程龙"), QStringLiteral("曹阳"), QStringLiteral("彭飞"),
    };

    buildUi();

    // 联网信令客户端（阶段三）：协议见 docs/信令协议.md
    m_netClient = new SignalingClient(this);
    connect(m_netClient, &SignalingClient::connected, this, &MainWindow::onNetConnected);
    connect(m_netClient, &SignalingClient::disconnected, this, &MainWindow::onNetDisconnected);
    connect(m_netClient, &SignalingClient::roomCreated, this, &MainWindow::onRoomCreated);
    connect(m_netClient, &SignalingClient::joined, this, &MainWindow::onJoined);
    connect(m_netClient, &SignalingClient::memberJoined, this, &MainWindow::onMemberJoined);
    connect(m_netClient, &SignalingClient::memberLeft, this, &MainWindow::onMemberLeft);
    connect(m_netClient, &SignalingClient::memberUpdated, this, &MainWindow::onMemberUpdated);
    connect(m_netClient, &SignalingClient::chatMessage, this, &MainWindow::onChatMessage);
    connect(m_netClient, &SignalingClient::roomClosed, this, &MainWindow::onRoomClosed);
    connect(m_netClient, &SignalingClient::errorOccurred, this, &MainWindow::onNetError);
    connect(m_netClient, &SignalingClient::mediaFrameReceived, this, &MainWindow::onMediaFrame);

    // 自动请求摄像头/麦克风权限：授权后主持人宫格自动从模拟画面升级为真实摄像头，
    // 拒绝或不可用时自动回退模拟画面，全程无需手动确认。
    requestAutoPermissions();

    // 初始成员：主持人 + 3 位成员 = 4 人（2x2 宫格）
    {
        MemberData self;
        self.name = QStringLiteral("主持人");
        self.color = QColor(0x2e, 0x8b, 0xff);
        self.isSelf = true;
        self.speaking = false; // 由真实音频说话检测驱动
        m_members.append(self);

        for (int i = 0; i < 3; ++i)
            m_members.append(nextMember());
    }

    // 支持启动参数 --members=N 演示指定规模宫格
    if (initialMembers > 0)
        setMemberCount(initialMembers);
    else
        setMemberCount(m_members.size()); // 触发滑块同步 + 重建 + 计数刷新

    // 会议计时
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::updateTimer);
    m_timer->start(1000);
    updateTimerLabel();

    setWindowTitle(QStringLiteral("MeetingGrid - 会议多宫格显示原型"));
    resize(1280, 800);
    setMinimumSize(960, 620);
}

// ---------------------------------------------------------------------------
// 界面构建
// ---------------------------------------------------------------------------

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    central->setStyleSheet(QStringLiteral(
        "QWidget { background: #f2f4f8; }"
        "QLabel { background: transparent; }"));

    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- 顶栏（无边框窗口下兼作拖拽区） ----
    m_topBar = new QWidget(central);
    m_topBar->setStyleSheet(QStringLiteral("QWidget { background: #ffffff; }"));
    m_topBar->setFixedHeight(52);
    auto *topLay = new QHBoxLayout(m_topBar);
    topLay->setContentsMargins(18, 0, 0, 0);
    topLay->setSpacing(14);

    m_titleLabel = new QLabel(QStringLiteral("线上会议"), m_topBar);
    m_titleLabel->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold; color: #23272e;"));

    m_meetingIdLabel = new QLabel(QStringLiteral("会议号：%1").arg(QLatin1String(kMeetingId)), m_topBar);
    m_meetingIdLabel->setStyleSheet(QStringLiteral("font-size: 12px; color: #6b7280;"));

    m_timerLabel = new QLabel(m_topBar);
    m_timerLabel->setStyleSheet(QStringLiteral("font-size: 12px; color: #6b7280;"));

    m_countBadge = new QLabel(m_topBar);
    m_countBadge->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #2e8bff; background: #e8f1ff;"
        "border-radius: 10px; padding: 3px 10px;"));

    topLay->addWidget(m_titleLabel);
    topLay->addWidget(m_meetingIdLabel);
    topLay->addStretch(1);
    topLay->addWidget(m_timerLabel);
    topLay->addWidget(m_countBadge);

    // 无边框窗口控制按钮：最小化 / 最大化（还原） / 关闭
    m_minBtn = new WindowButton(IconFactory::IconKind::Minimize, m_topBar);
    m_minBtn->setToolTip(QStringLiteral("最小化"));
    m_maxBtn = new WindowButton(IconFactory::IconKind::Maximize, m_topBar);
    m_maxBtn->setToolTip(QStringLiteral("最大化"));
    m_closeBtn = new WindowButton(IconFactory::IconKind::Close, m_topBar);
    m_closeBtn->setToolTip(QStringLiteral("关闭"));

    connect(m_minBtn, &QPushButton::clicked, this, &MainWindow::onWindowMinimize);
    connect(m_maxBtn, &QPushButton::clicked, this, &MainWindow::onWindowMaximizeRestore);
    connect(m_closeBtn, &QPushButton::clicked, this, &MainWindow::onWindowClose);

    topLay->addWidget(m_minBtn);
    topLay->addWidget(m_maxBtn);
    topLay->addWidget(m_closeBtn);

    updateWindowStateButtons();
    m_topBar->installEventFilter(this);

    root->addWidget(m_topBar);

    // ---- 中央：宫格 + 右侧成员面板 ----
    buildGridArea();
    root->addWidget(m_grid->parentWidget(), 1);

    // ---- 聊天面板（默认隐藏） ----
    buildChatPanel();
    root->addWidget(m_chatPanel);

    // ---- 底部控制栏 ----
    buildBottomBar();
    auto *bottomBar = new QWidget(central);
    bottomBar->setStyleSheet(QStringLiteral("QWidget { background: #ffffff; }"));
    bottomBar->setFixedHeight(72);
    auto *bottomLay = new QHBoxLayout(bottomBar);
    bottomLay->setContentsMargins(16, 6, 16, 6);
    bottomLay->setSpacing(10);

    bottomLay->addWidget(m_micBtn);
    bottomLay->addWidget(m_camBtn);
    bottomLay->addWidget(m_shareBtn);
    bottomLay->addWidget(m_chatBtn);
    bottomLay->addWidget(m_networkBtn);
    bottomLay->addWidget(m_viewBtn);
    bottomLay->addWidget(m_settingsBtn);
    bottomLay->addStretch(1);
    bottomLay->addWidget(m_endBtn);

    root->addWidget(bottomBar);

    setCentralWidget(central);
}

void MainWindow::buildGridArea()
{
    auto *center = new QWidget(this);
    center->setStyleSheet(QStringLiteral("QWidget { background: transparent; }"));
    auto *centerLay = new QHBoxLayout(center);
    centerLay->setContentsMargins(0, 0, 0, 0);
    centerLay->setSpacing(0);

    // 宫格区
    m_grid = new GridContainer(center);
    centerLay->addWidget(m_grid, 1);

    // 右侧成员管理面板
    buildRightPanel();
    m_rightPanel->setFixedWidth(270);
    centerLay->addWidget(m_rightPanel);

    m_grid->setParent(center);
}

void MainWindow::buildRightPanel()
{
    m_rightPanel = new QWidget(this);
    m_rightPanel->setStyleSheet(QStringLiteral("QWidget { background: #ffffff; }"));
    auto *lay = new QVBoxLayout(m_rightPanel);
    lay->setContentsMargins(14, 14, 14, 14);
    lay->setSpacing(10);

    // 标题
    auto *title = new QLabel(QStringLiteral("成员列表"), m_rightPanel);
    title->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold; color: #23272e;"));
    lay->addWidget(title);

    // 成员列表
    m_memberList = new QListWidget(m_rightPanel);
    m_memberList->setStyleSheet(QStringLiteral(
        "QListWidget { border: 1px solid #e6e8ec; border-radius: 8px; background: #fafbfc;"
        " font-size: 12px; }"
        "QListWidget::item { padding: 5px 8px; }"
        "QListWidget::item:selected { background: #e8f1ff; color: #23272e; }"));
    m_memberList->setSelectionMode(QAbstractItemView::NoSelection);
    m_memberList->setMinimumHeight(120);
    lay->addWidget(m_memberList, 1);

    // 分隔线
    auto *line = new QFrame(m_rightPanel);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QStringLiteral("color: #e6e8ec;"));
    lay->addWidget(line);

    // 本地演示控制区（联网模式下整体隐藏）
    m_demoSection = new QWidget(m_rightPanel);
    m_demoSection->setStyleSheet(QStringLiteral("QWidget { background: transparent; }"));
    auto *demoLay = new QVBoxLayout(m_demoSection);
    demoLay->setContentsMargins(0, 0, 0, 0);
    demoLay->setSpacing(10);

    // 动态宫格控制区
    auto *ctrlTitle = new QLabel(QStringLiteral("动态宫格演示"), m_demoSection);
    ctrlTitle->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold; color: #374151;"));
    demoLay->addWidget(ctrlTitle);

    // 滑块 + 数值
    auto *sliderRow = new QHBoxLayout;
    sliderRow->setSpacing(8);
    m_countSlider = new QSlider(Qt::Horizontal, m_demoSection);
    m_countSlider->setRange(1, 49);
    m_countSlider->setValue(4);
    m_countSlider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 6px; background: #dfe3ea; border-radius: 3px; }"
        "QSlider::handle:horizontal { width: 16px; margin: -5px 0; border-radius: 8px;"
        " background: #2e8bff; }"));
    m_sliderValue = new QLabel(QStringLiteral("当前 4 人"), m_demoSection);
    m_sliderValue->setStyleSheet(QStringLiteral("font-size: 12px; color: #374151;"));
    sliderRow->addWidget(m_countSlider, 1);
    sliderRow->addWidget(m_sliderValue);
    demoLay->addLayout(sliderRow);

    connect(m_countSlider, &QSlider::valueChanged, this, &MainWindow::onCountSliderChanged);

    // 快捷数量按钮
    auto *presetRow = new QHBoxLayout;
    presetRow->setSpacing(6);
    const int presets[] = {1, 4, 9, 16, 25};
    for (int v : presets) {
        auto *btn = new QPushButton(QString::number(v), m_demoSection);
        btn->setFixedHeight(26);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { border: 1px solid #d7dbe1; border-radius: 6px; font-size: 12px; color: #374151;"
            " background: #ffffff; }"
            "QPushButton:hover { border-color: #2e8bff; color: #2e8bff; }"));
        connect(btn, &QPushButton::clicked, this, [this, v]() { setMemberCount(v); });
        presetRow->addWidget(btn);
    }
    demoLay->addLayout(presetRow);

    // 添加 / 移除按钮
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    auto *addBtn = new ControlButton(IconFactory::IconKind::Plus, QStringLiteral("添加成员"), m_demoSection);
    addBtn->setIconSizeHint(18);
    addBtn->setFixedHeight(40);
    addBtn->setStyleSheet(QStringLiteral("QPushButton { background: #e8f1ff; border-radius: 8px; }"));

    auto *removeBtn = new ControlButton(IconFactory::IconKind::Minus, QStringLiteral("移除成员"), m_demoSection);
    removeBtn->setIconSizeHint(18);
    removeBtn->setFixedHeight(40);
    removeBtn->setStyleSheet(QStringLiteral("QPushButton { background: #fdecec; border-radius: 8px; }"));

    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addMember);
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeMember);

    btnRow->addWidget(addBtn, 1);
    btnRow->addWidget(removeBtn, 1);
    demoLay->addLayout(btnRow);

    lay->addWidget(m_demoSection);

    // 当前宫格提示
    m_gridHint = new QLabel(m_rightPanel);
    m_gridHint->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #2e8bff; background: #e8f1ff; border-radius: 6px; padding: 6px 8px;"));
    lay->addWidget(m_gridHint);

    lay->addStretch(1);

    // 说明
    m_demoNote = new QLabel(QStringLiteral("提示：拖动滑块或点击数量按钮，\n可实时演示宫格随成员数自动调整。"), m_rightPanel);
    m_demoNote->setStyleSheet(QStringLiteral("font-size: 11px; color: #9ca3af;"));
    m_demoNote->setWordWrap(true);
    lay->addWidget(m_demoNote);
}

void MainWindow::buildChatPanel()
{
    m_chatPanel = new QWidget(this);
    m_chatPanel->setStyleSheet(QStringLiteral("QWidget { background: #ffffff; }"));
    m_chatPanel->setFixedHeight(180);

    auto *lay = new QVBoxLayout(m_chatPanel);
    lay->setContentsMargins(12, 8, 12, 10);
    lay->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("聊天"), m_chatPanel);
    title->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold; color: #23272e;"));
    lay->addWidget(title);

    m_chatList = new QListWidget(m_chatPanel);
    m_chatList->setStyleSheet(QStringLiteral(
        "QListWidget { border: 1px solid #e6e8ec; border-radius: 8px; background: #fafbfc; font-size: 12px; }"
        "QListWidget::item { padding: 4px 6px; }"));
    lay->addWidget(m_chatList, 1);

    auto *row = new QHBoxLayout;
    row->setSpacing(6);
    m_chatEdit = new QLineEdit(m_chatPanel);
    m_chatEdit->setPlaceholderText(QStringLiteral("输入消息，回车发送..."));
    m_chatEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { border: 1px solid #d7dbe1; border-radius: 6px; padding: 6px 8px; font-size: 12px;"
        " background: #ffffff; }"
        "QLineEdit:focus { border-color: #2e8bff; }"));
    auto *sendBtn = new QPushButton(QStringLiteral("发送"), m_chatPanel);
    sendBtn->setCursor(Qt::PointingHandCursor);
    sendBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2e8bff; color: white; border: none; border-radius: 6px;"
        " padding: 6px 14px; font-size: 12px; }"
        "QPushButton:hover { background: #3a97ff; }"));
    row->addWidget(m_chatEdit, 1);
    row->addWidget(sendBtn);
    lay->addLayout(row);

    connect(sendBtn, &QPushButton::clicked, this, &MainWindow::onSendChat);
    connect(m_chatEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendChat);

    m_chatPanel->hide();
}

void MainWindow::buildBottomBar()
{
    m_micBtn = new ControlButton(IconFactory::IconKind::Mic, QStringLiteral("麦克风"), this);
    m_micBtn->setCheckable(true);
    m_micBtn->setChecked(true);

    m_camBtn = new ControlButton(IconFactory::IconKind::Camera, QStringLiteral("摄像头"), this);
    m_camBtn->setCheckable(true);
    m_camBtn->setChecked(true);

    m_shareBtn = new ControlButton(IconFactory::IconKind::Share, QStringLiteral("共享"), this);
    m_shareBtn->setCheckable(true);

    m_chatBtn = new ControlButton(IconFactory::IconKind::Chat, QStringLiteral("聊天"), this);

    m_networkBtn = new ControlButton(IconFactory::IconKind::Network, QStringLiteral("联网会议"), this);
    m_networkBtn->setCheckable(false);

    m_viewBtn = new ControlButton(IconFactory::IconKind::Grid, QStringLiteral("宫格视图"), this);

    m_settingsBtn = new ControlButton(IconFactory::IconKind::Settings, QStringLiteral("设置"), this);

    m_endBtn = new ControlButton(IconFactory::IconKind::EndCall, QStringLiteral("结束"), this);
    m_endBtn->setDanger(true);

    connect(m_micBtn, &QPushButton::toggled, this, &MainWindow::onMicToggle);
    connect(m_camBtn, &QPushButton::toggled, this, &MainWindow::onCamToggle);
    connect(m_shareBtn, &QPushButton::toggled, this, &MainWindow::onShareToggle);
    connect(m_chatBtn, &QPushButton::clicked, this, &MainWindow::onChatToggle);
    connect(m_networkBtn, &QPushButton::clicked, this, &MainWindow::onNetworkClicked);
    connect(m_viewBtn, &QPushButton::clicked, this, &MainWindow::onViewToggle);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);
    connect(m_endBtn, &QPushButton::clicked, this, &MainWindow::onEndCall);
}

// ---------------------------------------------------------------------------
// 成员管理
// ---------------------------------------------------------------------------

MainWindow::MemberData MainWindow::nextMember()
{
    MemberData m;
    m.name = m_namePool.at(m_nameCursor % m_namePool.size());
    m.color = m_colorPool.at(m_colorCursor % m_colorPool.size());
    m.micOn = (QRandomGenerator::global()->bounded(4) != 0);
    m.camOn = (QRandomGenerator::global()->bounded(5) != 0);
    m.speaking = false;
    ++m_nameCursor;
    ++m_colorCursor;
    return m;
}

VideoTile *MainWindow::makeTile(const MemberData &m, int index)
{
    auto *tile = new VideoTile(m.name, m.color, m.isSelf, index, m_grid);
    tile->setMicOn(m.micOn);
    tile->setCamOn(m.camOn);
    tile->setSpeaking(m.speaking);
    tile->setSharing(m.sharing);
    connect(tile, &VideoTile::clicked, this, &MainWindow::onTileClicked);
    return tile;
}

void MainWindow::rebuildTiles()
{
    // 释放上一轮的模拟视频源（避免泄漏 + 停掉定时器）
    releaseTransientSources();
    m_grid->clearTiles();
    m_remoteSources = QVector<VideoSource *>(m_members.size(), nullptr);

    for (int i = 0; i < m_members.size(); ++i) {
        VideoTile *tile = makeTile(m_members[i], i);
        tile->setFillMode(m_fillMode);
        m_grid->addTile(tile);

        VideoSource *src = nullptr;
        if (i == 0) {
            // 主持人：优先真实摄像头，否则模拟画面
            src = ensureSelfSource();
            if (m_members[i].camOn)
                src->start();
            else
                src->stop();
            // 网络推流（摄像头/模拟画面统一出帧）。先断开再连接，避免 rebuild 重复连接
            disconnect(src, &VideoSource::frameReady, this, &MainWindow::onSelfFrameForNetwork);
            connect(src, &VideoSource::frameReady, this, &MainWindow::onSelfFrameForNetwork);
        } else {
            // 远端成员：模拟画面占位；已收到真实画面的成员不再启动模拟源（避免闪烁）
            src = new SimulatedVideoSource(m_members[i].name, m_members[i].color, m_simulatedFps, this);
            m_transientSources.append(src);
            m_remoteSources[i] = src;
            if (m_members[i].camOn && !m_realVideoMembers.contains(m_members[i].id))
                src->start();
        }

        // 视频帧 -> 宫格
        connect(src, &VideoSource::frameReady, tile, [tile](const QImage &frame) {
            tile->setVideoFrame(frame);
        });
    }

    if (m_selectedIndex >= m_members.size())
        m_selectedIndex = m_members.isEmpty() ? -1 : 0;
    applySpeaking();
    updateMemberList();
    updateCountLabel();
    m_grid->relayout();
}

void MainWindow::releaseTransientSources()
{
    for (VideoSource *s : m_transientSources) {
        s->stop();
        s->deleteLater();
    }
    m_transientSources.clear();
}

VideoSource *MainWindow::ensureSelfSource()
{
    if (m_selfSource)
        return m_selfSource;

    const QList<QCameraDevice> cams = QMediaDevices::videoInputs();
    const QColor selfColor = m_members.isEmpty() ? QColor(0x2e, 0x8b, 0xff) : m_members[0].color;
    const QString selfName = m_members.isEmpty() ? QStringLiteral("主持人") : m_members[0].name;

    // 摄像头权限已授权 -> 直接使用摄像头；未授权/未定 -> 先用模拟画面，授权回调后自动升级
    if (!cams.isEmpty() && cameraPermissionGranted()) {
        createCameraSelfSource(cams.first());
        return m_selfSource;
    }
    m_selfSource = new SimulatedVideoSource(selfName, selfColor, m_simulatedFps, this);
    return m_selfSource;
}

void MainWindow::createCameraSelfSource(const QCameraDevice &device)
{
    auto *cam = new CameraVideoSource(device, this);
    connect(cam, &VideoSource::errorOccurred, this, &MainWindow::onCameraError);
    m_selfSource = cam;
    m_selectedCameraId = device.id();

    // 若主持人摄像头处于开启状态，立即把新源接回当前宫格并启动
    if (!m_members.isEmpty() && m_members[0].camOn) {
        if (VideoTile *t = m_grid->tileAt(0)) {
            connect(m_selfSource, &VideoSource::frameReady, t,
                    [t](const QImage &frame) { t->setVideoFrame(frame); });
            t->setFillMode(m_fillMode);
        }
        m_selfSource->start();
    }
}

void MainWindow::recreateSelfCamera(const QCameraDevice &device)
{
    if (m_selfSource) {
        m_selfSource->stop();
        m_selfSource->deleteLater();
        m_selfSource = nullptr;
    }
    createCameraSelfSource(device);
}

void MainWindow::upgradeSelfToCamera()
{
    // 已在用摄像头则跳过
    if (m_selfSource && qobject_cast<CameraVideoSource *>(m_selfSource))
        return;
    const QList<QCameraDevice> cams = QMediaDevices::videoInputs();
    if (cams.isEmpty())
        return;

    // 停用旧的模拟源
    if (m_selfSource) {
        m_selfSource->stop();
        m_selfSource->deleteLater();
        m_selfSource = nullptr;
    }
    createCameraSelfSource(cams.first());
}

void MainWindow::requestAutoPermissions()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // 摄像头：授权后自动从模拟画面升级为真实摄像头
    if (cameraPermissionGranted())
        upgradeSelfToCamera();
    else if (QCoreApplication::instance()->checkPermission(QCameraPermission())
             == Qt::PermissionStatus::Undetermined) {
        QCoreApplication::instance()->requestPermission(QCameraPermission(), [this](const QPermission &p) {
            if (p.status() == Qt::PermissionStatus::Granted)
                upgradeSelfToCamera();
        });
    }
    // 麦克风：授权后若麦克风处于开启状态，自动启动真实音频采集
    const auto micPerm = QCoreApplication::instance()->checkPermission(QMicrophonePermission());
    if (micPerm == Qt::PermissionStatus::Granted) {
        if (m_micBtn && m_micBtn->isChecked())
            startAudioCapture();
    } else if (micPerm == Qt::PermissionStatus::Undetermined) {
        QCoreApplication::instance()->requestPermission(QMicrophonePermission(), [this](const QPermission &p) {
            if (p.status() == Qt::PermissionStatus::Granted && m_micBtn && m_micBtn->isChecked())
                startAudioCapture();
        });
    }
#endif
}

// ---------------------------------------------------------------------------
// 音频采集（真实麦克风 + 说话检测）
// ---------------------------------------------------------------------------

void MainWindow::startAudioCapture()
{
    if (!m_audioSource) {
        m_audioSource = new AudioLevelSource(this);
        connect(m_audioSource, &AudioLevelSource::speakingChanged,
                this, &MainWindow::onSelfSpeakingChanged);
        connect(m_audioSource, &AudioLevelSource::audioData,
                this, &MainWindow::onSelfAudioData);
        connect(m_audioSource, &AudioLevelSource::errorOccurred,
                this, [this](const QString &) {
                    // 采集失败（无设备 / 权限被拒 / 设备被拔）：静默降级，界面保持可用
                    stopAudioCapture();
                });
    }
    m_audioSource->start();
}

void MainWindow::stopAudioCapture()
{
    if (m_audioSource)
        m_audioSource->stop();
}

void MainWindow::onSelfSpeakingChanged(bool speaking)
{
    if (m_members.isEmpty())
        return;
    m_members[0].speaking = speaking;
    if (VideoTile *t = m_grid->tileAt(0))
        t->setSpeaking(speaking);
    updateMemberList();
}

void MainWindow::onCameraError(const QString &message)
{
    // 摄像头打开失败 -> 回退到模拟画面，保证界面始终有内容
    if (m_selfSource && !qobject_cast<SimulatedVideoSource *>(m_selfSource)) {
        m_selfSource->stop();
        m_selfSource->deleteLater();
        m_selfSource = nullptr;

        const QColor selfColor = m_members.isEmpty() ? QColor(0x2e, 0x8b, 0xff) : m_members[0].color;
        const QString selfName = m_members.isEmpty() ? QStringLiteral("主持人") : m_members[0].name;
        m_selfSource = new SimulatedVideoSource(selfName, selfColor, m_simulatedFps, this);

        if (!m_members.isEmpty() && m_members[0].camOn) {
            if (VideoTile *t = m_grid->tileAt(0)) {
                connect(m_selfSource, &VideoSource::frameReady, t,
                        [t](const QImage &frame) { t->setVideoFrame(frame); });
                t->setFillMode(m_fillMode);
            }
            m_selfSource->start();
        }
        setWindowTitle(QStringLiteral("MeetingGrid - 摄像头不可用，已切换模拟画面"));
    }
    Q_UNUSED(message);
}

void MainWindow::addMember()
{
    setMemberCount(m_members.size() + 1);
}

void MainWindow::removeMember()
{
    if (m_members.size() <= 1)
        return;
    setMemberCount(m_members.size() - 1);
}

void MainWindow::setMemberCount(int n)
{
    n = qBound(1, n, 49);

    // 同步滑块（阻断信号，避免递归触发 onCountSliderChanged）
    const bool wasBlocked = m_countSlider->signalsBlocked();
    m_countSlider->blockSignals(true);
    m_countSlider->setValue(n);
    m_countSlider->blockSignals(wasBlocked);

    while (m_members.size() < n)
        m_members.append(nextMember());
    while (m_members.size() > n)
        m_members.removeLast();

    m_sliderValue->setText(QStringLiteral("当前 %1 人").arg(n));
    rebuildTiles();
}

void MainWindow::onCountSliderChanged(int value)
{
    setMemberCount(value);
}

void MainWindow::onTileClicked(int index)
{
    m_selectedIndex = index;

    // 模拟"说话"：被点击者高亮，其余停止
    for (int i = 0; i < m_members.size(); ++i)
        m_members[i].speaking = (i == index);
    applySpeaking();
    updateMemberList();

    if (m_grid->viewMode() == GridContainer::ViewMode::Speaker)
        m_grid->setSpeakerIndex(index);
}

// ---------------------------------------------------------------------------
// 状态同步
// ---------------------------------------------------------------------------

void MainWindow::updateCountLabel()
{
    const int n = m_members.size();
    m_countBadge->setText(QStringLiteral("成员 %1").arg(n));

    const GridManager::GridSpec spec = GridManager::calcGrid(n);
    if (m_grid->viewMode() == GridContainer::ViewMode::Speaker)
        m_gridHint->setText(QStringLiteral("演讲者视图 · %1 人").arg(n));
    else
        m_gridHint->setText(QStringLiteral("宫格 %1×%2 · %3 人").arg(spec.rows).arg(spec.cols).arg(n));
}

void MainWindow::updateMemberList()
{
    if (!m_memberList)
        return;
    m_memberList->clear();
    for (const MemberData &m : m_members) {
        QString status = QStringLiteral("%1 %2")
                             .arg(m.micOn ? QStringLiteral("麦克风开") : QStringLiteral("麦克风关"))
                             .arg(m.camOn ? QStringLiteral("视频开") : QStringLiteral("视频关"));
        if (m.sharing)
            status += QStringLiteral(" · 共享中");
        const QString host = m.isHost ? QStringLiteral("（主持）") : QString();
        const QString text = m.isSelf
                                 ? QStringLiteral("%1%2（我）| %3").arg(m.name, host, status)
                                 : QStringLiteral("%1%2 | %3").arg(m.name, host, status);
        auto *item = new QListWidgetItem(text, m_memberList);
        item->setForeground(m.speaking ? QColor(0x2e, 0xc4, 0x6b) : QColor(0x44, 0x4a, 0x52));
    }
}

void MainWindow::updateSelfTileState()
{
    if (m_members.isEmpty())
        return;
    MemberData &self = m_members[0];
    self.micOn = m_micBtn->isChecked();
    self.camOn = m_camBtn->isChecked();

    if (VideoTile *t = m_grid->tileAt(0)) {
        t->setMicOn(self.micOn);
        t->setCamOn(self.camOn);
        if (!self.camOn)
            t->clearVideoFrame(); // 关闭摄像头后不再显示残留画面
    }
    updateMemberList();
}

void MainWindow::onMicToggle(bool on)
{
    updateSelfTileState();
    // 真实控制麦克风采集：开 -> 启动音频源 + 说话检测；关 -> 停止
    if (on)
        startAudioCapture();
    else
        stopAudioCapture();
    // 联网模式：把本机麦状态同步给服务器
    if (m_online && m_netClient)
        m_netClient->setState(m_micBtn->isChecked(), m_camBtn->isChecked());
}

void MainWindow::onCamToggle(bool on)
{
    // 真实控制主持人视频源：开 -> 启动采集/模拟；关 -> 停止
    if (m_selfSource) {
        if (on)
            m_selfSource->start();
        else
            m_selfSource->stop();
    }
    updateSelfTileState();
    // 联网模式：把本机摄像状态同步给服务器
    if (m_online && m_netClient)
        m_netClient->setState(m_micBtn->isChecked(), m_camBtn->isChecked());
}

void MainWindow::onShareToggle(bool on)
{
    m_sharing = on;
    updateShareUi();
    // 联网模式：把本机共享状态同步给服务器
    if (m_online && m_netClient)
        m_netClient->setShare(on);

    if (on) {
        // 共享时自动暂停摄像头（共享优先），结束共享时恢复
        if (m_online && m_camBtn->isChecked()) {
            m_camBtn->setChecked(false); // 触发 onCamToggle(false)：停采集 + 同步状态
            m_shareAutoPausedCam = true;
        }
        if (m_online)
            startScreenSharing();
    } else {
        if (m_shareAutoPausedCam) {
            m_camBtn->setChecked(true); // 触发 onCamToggle(true)：恢复摄像头
            m_shareAutoPausedCam = false;
        }
        stopScreenSharing();
    }
}

void MainWindow::updateShareUi()
{
    if (m_sharing) {
        m_grid->setStyleSheet(QStringLiteral("GridContainer { border: 2px solid #2e8bff; }"));
        setWindowTitle(QStringLiteral("MeetingGrid - 正在共享屏幕"));
    } else {
        m_grid->setStyleSheet(QString());
        setWindowTitle(QStringLiteral("MeetingGrid - 会议多宫格显示原型"));
    }
}

void MainWindow::onViewToggle()
{
    if (m_grid->viewMode() == GridContainer::ViewMode::Gallery) {
        if (m_selectedIndex < 0)
            m_selectedIndex = 0;
        m_grid->setViewMode(GridContainer::ViewMode::Speaker);
        m_grid->setSpeakerIndex(m_selectedIndex);
    } else {
        m_grid->setViewMode(GridContainer::ViewMode::Gallery);
    }
    updateViewButton();
    updateCountLabel();
}

void MainWindow::updateViewButton()
{
    if (m_grid->viewMode() == GridContainer::ViewMode::Speaker) {
        m_viewBtn->setIconKind(IconFactory::IconKind::Speaker);
        m_viewBtn->setLabelText(QStringLiteral("演讲者视图"));
    } else {
        m_viewBtn->setIconKind(IconFactory::IconKind::Grid);
        m_viewBtn->setLabelText(QStringLiteral("宫格视图"));
    }
}

void MainWindow::applySpeaking()
{
    const int n = qMin(m_members.size(), m_grid->tileCount());
    for (int i = 0; i < n; ++i) {
        if (VideoTile *t = m_grid->tileAt(i))
            t->setSpeaking(m_members[i].speaking);
    }
}

// ---------------------------------------------------------------------------
// 设置面板
// ---------------------------------------------------------------------------

void MainWindow::openSettings()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("设置"));
    dlg.setMinimumWidth(320);

    auto *form = new QFormLayout(&dlg);
    form->setContentsMargins(20, 16, 20, 16);
    form->setSpacing(12);

    // 权限状态（自动处理结果展示）
    QString camStatus, micStatus;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const auto camPerm = QCoreApplication::instance()->checkPermission(QCameraPermission());
    const auto micPerm = QCoreApplication::instance()->checkPermission(QMicrophonePermission());
    auto desc = [](Qt::PermissionStatus st) -> QString {
        if (st == Qt::PermissionStatus::Granted)
            return QStringLiteral("已允许（自动）");
        if (st == Qt::PermissionStatus::Denied)
            return QStringLiteral("已拒绝（已回退模拟画面）");
        return QStringLiteral("待确认");
    };
    camStatus = desc(camPerm);
    micStatus = desc(micPerm);
#else
    camStatus = micStatus = QStringLiteral("自动");
#endif
    auto *permLabel = new QLabel(QStringLiteral("摄像头：%1　　麦克风：%2")
                                     .arg(camStatus, micStatus), &dlg);
    permLabel->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #2e8bff; background: #e8f1ff; border-radius: 6px; padding: 6px 10px;"));
    permLabel->setWordWrap(true);
    form->addRow(permLabel);

    // 摄像头选择
    auto *camCombo = new QComboBox(&dlg);
    const QList<QCameraDevice> cams = QMediaDevices::videoInputs();
    int camIndex = 0;
    if (cams.isEmpty()) {
        camCombo->addItem(QStringLiteral("未检测到摄像头"), QString());
    } else {
        for (int i = 0; i < cams.size(); ++i) {
            camCombo->addItem(cams[i].description(), cams[i].id());
            if (cams[i].id() == m_selectedCameraId)
                camIndex = i;
        }
        camCombo->setCurrentIndex(camIndex);
    }

    // 麦克风设备选择
    auto *micCombo = new QComboBox(&dlg);
    const QList<QAudioDevice> mics = QMediaDevices::audioInputs();
    const QString curMicId = m_audioSource ? m_audioSource->device().id()
                                           : QMediaDevices::defaultAudioInput().id();
    int micIndex = 0;
    if (mics.isEmpty()) {
        micCombo->addItem(QStringLiteral("未检测到音频输入设备"), QString());
    } else {
        for (int i = 0; i < mics.size(); ++i) {
            micCombo->addItem(mics[i].description(), mics[i].id());
            if (mics[i].id() == curMicId)
                micIndex = i;
        }
        micCombo->setCurrentIndex(micIndex);
    }

    // 视频填充模式
    auto *fillCombo = new QComboBox(&dlg);
    fillCombo->addItem(QStringLiteral("裁剪填充（铺满宫格）"), int(VideoTile::FillMode::Crop));
    fillCombo->addItem(QStringLiteral("等比缩放（完整显示）"), int(VideoTile::FillMode::Fit));
    fillCombo->setCurrentIndex(m_fillMode == VideoTile::FillMode::Crop ? 0 : 1);

    // 模拟画面帧率
    auto *fpsSpin = new QSpinBox(&dlg);
    fpsSpin->setRange(1, 30);
    fpsSpin->setValue(m_simulatedFps);
    fpsSpin->setSuffix(QStringLiteral(" fps"));

    form->addRow(QStringLiteral("摄像头"), camCombo);
    form->addRow(QStringLiteral("麦克风设备"), micCombo);
    form->addRow(QStringLiteral("视频填充"), fillCombo);
    form->addRow(QStringLiteral("模拟画面帧率"), fpsSpin);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btnBox);

    if (dlg.exec() != QDialog::Accepted)
        return;

    // 应用填充模式
    m_fillMode = VideoTile::FillMode(fillCombo->currentData().toInt());
    // 应用帧率
    m_simulatedFps = fpsSpin->value();
    // 应用摄像头切换
    const QString camId = camCombo->currentData().toString();
    if (!camId.isEmpty() && camId != m_selectedCameraId) {
        for (const QCameraDevice &dev : QMediaDevices::videoInputs()) {
            if (dev.id() == camId) {
                recreateSelfCamera(dev);
                break;
            }
        }
    }

    // 应用麦克风设备切换
    const QString micId = micCombo->currentData().toString();
    if (!micId.isEmpty()) {
        for (const QAudioDevice &dev : QMediaDevices::audioInputs()) {
            if (dev.id() == micId) {
                if (m_micBtn->isChecked())
                    startAudioCapture(); // 确保音频源存在
                if (m_audioSource)
                    m_audioSource->setDevice(dev);
                break;
            }
        }
    }

    applyViewSettings();
}

void MainWindow::applyViewSettings()
{
    // 填充模式应用到所有宫格
    for (int i = 0; i < m_grid->tileCount(); ++i) {
        if (VideoTile *t = m_grid->tileAt(i))
            t->setFillMode(m_fillMode);
    }
    // 帧率应用到所有模拟源
    for (VideoSource *s : m_transientSources) {
        if (auto *sim = qobject_cast<SimulatedVideoSource *>(s))
            sim->setFps(m_simulatedFps);
    }
    if (auto *selfSim = qobject_cast<SimulatedVideoSource *>(m_selfSource))
        selfSim->setFps(m_simulatedFps);
}

// ---------------------------------------------------------------------------
// 聊天 / 计时 / 结束
// ---------------------------------------------------------------------------

void MainWindow::onChatToggle()
{
    m_chatPanel->setVisible(!m_chatPanel->isVisible());
}

void MainWindow::onSendChat()
{
    const QString msg = m_chatEdit->text().trimmed();
    if (msg.isEmpty())
        return;
    if (m_online && m_netClient && m_netClient->isInRoom()) {
        // 联网模式：发给服务器，由广播回显统一显示（含发送者名字）
        m_netClient->sendChat(msg);
    } else {
        // 本地演示模式：直接回显
        m_chatList->addItem(QStringLiteral("[主持人] ") + msg);
        m_chatList->scrollToBottom();
    }
    m_chatEdit->clear();
}

void MainWindow::updateTimer()
{
    ++m_elapsed;
    updateTimerLabel();
}

void MainWindow::updateTimerLabel()
{
    const int mm = m_elapsed / 60;
    const int ss = m_elapsed % 60;
    m_timerLabel->setText(QStringLiteral("已进行 %1:%2")
                              .arg(mm, 2, 10, QLatin1Char('0'))
                              .arg(ss, 2, 10, QLatin1Char('0')));
}

void MainWindow::onEndCall()
{
    if (m_online) {
        // 联网模式：结束 = 离开会议（主持人离开后服务器向其余人广播 room_closed）
        const auto ret = QMessageBox::question(
            this, QStringLiteral("离开会议"),
            QStringLiteral("确定要离开当前会议吗？\n主持人离开后会议将结束。"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::Yes)
            leaveNetworkMeeting();
        return;
    }
    const auto ret = QMessageBox::question(
        this, QStringLiteral("结束会议"),
        QStringLiteral("确定要结束会议吗？\n所有成员将退出会议。"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret == QMessageBox::Yes)
        QApplication::quit();
}

// ---------------------------------------------------------------------------
// 无边框窗口：拖拽 / 双击最大化 / 右键菜单 / 窗口控制按钮 / 边缘缩放
// ---------------------------------------------------------------------------

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // 只处理顶栏及其子控件（窗口控制按钮等 QAbstractButton 除外，交给按钮自身）
    if (obj == m_topBar || obj->parent() == m_topBar) {
        if (qobject_cast<QAbstractButton *>(obj))
            return QMainWindow::eventFilter(obj, event);

        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
                m_dragging = true;
                return true;
            } else if (me->button() == Qt::RightButton) {
                showTopBarMenu(me->globalPosition().toPoint());
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if (m_dragging) {
                auto *me = static_cast<QMouseEvent *>(event);
                if (!isMaximized())
                    move(me->globalPosition().toPoint() - m_dragOffset);
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            if (m_dragging) {
                m_dragging = false;
                return true;
            }
            break;
        }
        case QEvent::MouseButtonDblClick: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                onWindowMaximizeRestore();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
        updateWindowStateButtons();
}

void MainWindow::onWindowMinimize()
{
    showMinimized();
}

void MainWindow::onWindowMaximizeRestore()
{
    if (isMaximized())
        showNormal();
    else
        showMaximized();
}

void MainWindow::onWindowClose()
{
    close();
}

void MainWindow::updateWindowStateButtons()
{
    if (!m_maxBtn)
        return;
    const bool maximized = isMaximized();
    m_maxBtn->setIconKind(maximized ? IconFactory::IconKind::Restore
                                    : IconFactory::IconKind::Maximize);
    m_maxBtn->setToolTip(maximized ? QStringLiteral("还原") : QStringLiteral("最大化"));
}

void MainWindow::showTopBarMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    QAction *restore = menu.addAction(QStringLiteral("还原"));
    restore->setEnabled(isMaximized());
    QAction *minimize = menu.addAction(QStringLiteral("最小化"));
    QAction *maximize = menu.addAction(QStringLiteral("最大化"));
    maximize->setEnabled(!isMaximized());
    menu.addSeparator();
    QAction *close = menu.addAction(QStringLiteral("关闭"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == restore || chosen == maximize)
        onWindowMaximizeRestore();
    else if (chosen == minimize)
        onWindowMinimize();
    else if (chosen == close)
        onWindowClose();
}

// Windows：通过 WM_NCHITTEST 让无边框窗口支持边缘拖拽缩放（保留标准缩放光标）
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#if defined(Q_OS_WIN)
    if (eventType == QByteArrayLiteral("windows_generic_MSGN")) {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_NCHITTEST && !isMaximized()) {
            const QPoint pos = mapFromGlobal(QCursor::pos());
            const int x = pos.x();
            const int y = pos.y();
            const int w = width();
            const int h = height();
            const int b = 6; // 边缘热区宽度
            int ht = HTCLIENT;
            const bool left = x < b, right = x >= w - b;
            const bool top = y < b, bottom = y >= h - b;
            if (left && top)          ht = HTTOPLEFT;
            else if (right && top)    ht = HTTOPRIGHT;
            else if (left && bottom)  ht = HTBOTTOMLEFT;
            else if (right && bottom) ht = HTBOTTOMRIGHT;
            else if (left)            ht = HTLEFT;
            else if (right)           ht = HTRIGHT;
            else if (top)             ht = HTTOP;
            else if (bottom)          ht = HTBOTTOM;
            if (ht != HTCLIENT) {
                *result = ht;
                return true;
            }
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

// ---------------------------------------------------------------------------
// 联网会议（阶段三 · 信令接入）
// ---------------------------------------------------------------------------

void MainWindow::onNetworkClicked()
{
    if (m_online) {
        leaveNetworkMeeting();
        return;
    }
    promptNetworkConnection();
}

void MainWindow::promptNetworkConnection()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("联网会议"));
    dlg.setMinimumWidth(380);
    auto *form = new QFormLayout(&dlg);
    form->setContentsMargins(20, 16, 20, 16);
    form->setSpacing(12);

    auto *serverEdit = new QLineEdit(
        m_netServerUrl.isEmpty() ? QStringLiteral("ws://127.0.0.1:8090") : m_netServerUrl, &dlg);
    serverEdit->setPlaceholderText(QStringLiteral("ws://主机:端口"));
    auto *roomEdit = new QLineEdit(&dlg);
    roomEdit->setPlaceholderText(QStringLiteral("留空则创建新会议"));
    auto *nameEdit = new QLineEdit(QStringLiteral("主持人"), &dlg);

    form->addRow(QStringLiteral("服务端地址"), serverEdit);
    form->addRow(QStringLiteral("会议号"), roomEdit);
    form->addRow(QStringLiteral("你的名字"), nameEdit);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    auto *createBtn = new QPushButton(QStringLiteral("创建会议"), &dlg);
    auto *joinBtn = new QPushButton(QStringLiteral("加入会议"), &dlg);
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), &dlg);
    btnRow->addWidget(createBtn);
    btnRow->addWidget(joinBtn);
    btnRow->addWidget(cancelBtn);
    form->addRow(btnRow);

    int action = 0; // 1=创建, 2=加入
    connect(createBtn, &QPushButton::clicked, &dlg, [&]() { action = 1; dlg.accept(); });
    connect(joinBtn, &QPushButton::clicked, &dlg, [&]() {
        if (roomEdit->text().trimmed().isEmpty()) {
            roomEdit->setFocus();
            return;
        }
        action = 2;
        dlg.accept();
    });
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted || action == 0)
        return;

    m_netServerUrl = serverEdit->text().trimmed();
    m_pendingName = nameEdit->text().trimmed();
    if (m_pendingName.isEmpty())
        m_pendingName = QStringLiteral("主持人");
    m_pendingRoomId = (action == 2) ? roomEdit->text().trimmed() : QString();

    // 地址规范化：缺协议前缀时补 ws://
    if (!m_netServerUrl.startsWith(QLatin1String("ws://"))
        && !m_netServerUrl.startsWith(QLatin1String("wss://")))
        m_netServerUrl.prepend(QStringLiteral("ws://"));

    if (m_netClient->isConnected())
        applyPendingAction();
    else
        m_netClient->connectToServer(m_netServerUrl);
}

void MainWindow::applyPendingAction()
{
    if (m_pendingRoomId.isEmpty())
        m_netClient->createRoom(m_pendingName);
    else
        m_netClient->joinRoom(m_pendingRoomId, m_pendingName);
    m_pendingRoomId.clear();
}

void MainWindow::onNetConnected()
{
    applyPendingAction();
}

void MainWindow::onNetDisconnected()
{
    // 断开（主动或异常）→ 回本地演示模式
    setOnlineMode(false);
}

void MainWindow::onNetError(const QString &code, const QString &message)
{
    Q_UNUSED(code);
    QMessageBox::warning(this, QStringLiteral("联网会议"), message);
}

void MainWindow::onRoomCreated(const QString &roomId)
{
    Q_UNUSED(roomId);
    // 服务端随后下发 joined，成员列表在那里统一重建
}

void MainWindow::onJoined(const QString &roomId, const QString &selfId,
                          const QVector<SignalingClient::Member> &members)
{
    Q_UNUSED(roomId);
    setOnlineMode(true);
    populateFromNetwork(members, selfId);
    updateMeetingIdLabel();
}

void MainWindow::onMemberJoined(const SignalingClient::Member &member)
{
    if (!m_online)
        return;
    MemberData md;
    md.id = member.id;
    md.name = member.name;
    md.color = colorForNetworkId(member.id);
    md.micOn = member.micOn;
    md.camOn = member.camOn;
    md.sharing = member.sharing;
    md.isHost = member.isHost;
    md.isSelf = false;
    m_members.append(md);
    rebuildTiles();
}

void MainWindow::onMemberLeft(const QString &memberId)
{
    if (!m_online)
        return;
    m_realVideoMembers.remove(memberId); // 成员离开后清除其"真实画面"标记
    for (int i = 0; i < m_members.size(); ++i) {
        if (m_members[i].id == memberId) {
            m_members.removeAt(i);
            break;
        }
    }
    rebuildTiles();
}

void MainWindow::onMemberUpdated(const QString &memberId, bool mic, bool cam, bool sharing)
{
    if (!m_online)
        return;
    int idx = -1;
    for (int i = 0; i < m_members.size(); ++i) {
        if (m_members[i].id == memberId) {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return;
    m_members[idx].micOn = mic;
    m_members[idx].camOn = cam;
    m_members[idx].sharing = sharing;
    if (VideoTile *t = m_grid->tileAt(idx)) {
        t->setMicOn(mic);
        t->setCamOn(cam);
        t->setSharing(sharing);
        if (!cam)
            t->clearVideoFrame(); // 关闭摄像头后不再显示残留画面
    }
    updateMemberList();
}

void MainWindow::onChatMessage(const QString &memberId, const QString &name, const QString &text)
{
    Q_UNUSED(memberId);
    m_chatList->addItem(QStringLiteral("[%1] %2").arg(name, text));
    m_chatList->scrollToBottom();
}

void MainWindow::onRoomClosed()
{
    QMessageBox::information(this, QStringLiteral("会议结束"),
                             QStringLiteral("会议已结束（主持人已离开）。"));
    if (m_netClient)
        m_netClient->disconnectFromServer(); // 触发 onNetDisconnected → 回本地演示
}

void MainWindow::setOnlineMode(bool online)
{
    const bool changed = (m_online != online);
    m_online = online;
    if (m_demoSection)
        m_demoSection->setVisible(!online);
    if (m_demoNote)
        m_demoNote->setVisible(!online);
    if (m_networkBtn)
        m_networkBtn->setLabelText(online ? QStringLiteral("离开会议")
                                          : QStringLiteral("联网会议"));
    if (!online) {
        stopScreenSharing();      // 停屏幕推流
        teardownRemoteAudio();    // 停远端音频播放
        if (changed) {
            m_netColorOf.clear();
            resetLocalDemoMembers();
        }
    }
}

void MainWindow::populateFromNetwork(const QVector<SignalingClient::Member> &list,
                                     const QString &selfId)
{
    QVector<MemberData> result;
    MemberData self;
    for (const SignalingClient::Member &m : list) {
        MemberData md;
        md.id = m.id;
        md.name = m.name;
        md.color = colorForNetworkId(m.id);
        md.micOn = m.micOn;
        md.camOn = m.camOn;
        md.sharing = m.sharing;
        md.speaking = false;
        md.isHost = m.isHost;
        md.isSelf = (m.id == selfId);
        if (md.isSelf)
            self = md;
        else
            result.append(md);
    }
    // 不变量：m_members[0] 恒为本机自己
    if (!self.id.isEmpty())
        result.prepend(self);
    m_members = result;

    if (m_selectedIndex >= m_members.size())
        m_selectedIndex = m_members.isEmpty() ? -1 : 0;
    rebuildTiles();
}

void MainWindow::resetLocalDemoMembers()
{
    m_members.clear();
    MemberData self;
    self.name = QStringLiteral("主持人");
    self.color = QColor(0x2e, 0x8b, 0xff);
    self.isSelf = true;
    m_members.append(self);
    m_nameCursor = 0;
    m_colorCursor = 0;
    for (int i = 0; i < 3; ++i)
        m_members.append(nextMember());
    setMemberCount(m_members.size()); // 同步滑块 + 重建宫格
}

void MainWindow::leaveNetworkMeeting()
{
    if (!m_netClient)
        return;
    if (m_netClient->isInRoom())
        m_netClient->leaveRoom();
    m_netClient->disconnectFromServer();
    setOnlineMode(false);
}

void MainWindow::updateMeetingIdLabel()
{
    if (m_online && m_netClient && m_netClient->roomId().size() == 9) {
        const QString rid = m_netClient->roomId();
        m_meetingIdLabel->setText(QStringLiteral("会议号：%1 %2 %3")
                                      .arg(rid.left(3), rid.mid(3, 3), rid.right(3)));
    } else {
        m_meetingIdLabel->setText(QStringLiteral("会议号：%1").arg(QLatin1String(kMeetingId)));
    }
}

QColor MainWindow::colorForNetworkId(const QString &id)
{
    auto it = m_netColorOf.constFind(id);
    if (it != m_netColorOf.constEnd())
        return it.value();
    const QColor c = m_colorPool.at(m_colorCursor % m_colorPool.size());
    ++m_colorCursor;
    m_netColorOf.insert(id, c);
    return c;
}

// ---------------------------------------------------------------------------
// 媒体传输（WebSocket 帧中继）：推视频 / 推音频 / 屏幕共享 / 收流
// ---------------------------------------------------------------------------

void MainWindow::onSelfFrameForNetwork(const QImage &frame)
{
    if (!m_online || !m_netClient || !m_netClient->isInRoom())
        return;
    if (m_sharingScreen) // 共享期间只推屏幕
        return;
    if (!m_members.isEmpty() && !m_members[0].camOn)
        return;

    // 节流 ~10fps
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastSelfVideoMs < 100)
        return;
    m_lastSelfVideoMs = now;

    QImage img = frame;
    if (img.width() > 640)
        img = img.scaledToWidth(640, Qt::SmoothTransformation);

    QJsonObject meta;
    meta[QLatin1String(Protocol::KeyKind)] = QLatin1String(Protocol::MediaKind::Video);
    meta[QLatin1String(Protocol::KeyW)] = img.width();
    meta[QLatin1String(Protocol::KeyH)] = img.height();
    m_netClient->sendMedia(meta, encodeJpeg(img, 65));
}

void MainWindow::onSelfAudioData(const QByteArray &pcm)
{
    if (!m_online || !m_netClient || !m_netClient->isInRoom())
        return;
    if (!m_members.isEmpty() && !m_members[0].micOn)
        return;
    QJsonObject meta;
    meta[QLatin1String(Protocol::KeyKind)] = QLatin1String(Protocol::MediaKind::Audio);
    m_netClient->sendMedia(meta, pcm);
}

void MainWindow::grabAndSendScreen()
{
    if (!m_online || !m_netClient || !m_netClient->isInRoom())
        return;
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    QPixmap pm = screen->grabWindow(0);
    if (pm.isNull())
        return;
    QImage img = pm.toImage();
    if (img.width() > 1280)
        img = img.scaledToWidth(1280, Qt::SmoothTransformation);

    QJsonObject meta;
    meta[QLatin1String(Protocol::KeyKind)] = QLatin1String(Protocol::MediaKind::Screen);
    meta[QLatin1String(Protocol::KeyW)] = img.width();
    meta[QLatin1String(Protocol::KeyH)] = img.height();
    m_netClient->sendMedia(meta, encodeJpeg(img, 55));

    // 本地预览：self 宫格显示屏幕
    if (!m_members.isEmpty() && m_grid->tileAt(0))
        m_grid->tileAt(0)->setVideoFrame(img);
}

void MainWindow::onMediaFrame(const QString &kind, const QString &senderId,
                              const QByteArray &payload, const QJsonObject &meta)
{
    Q_UNUSED(meta);
    if (!m_online)
        return;
    const int idx = memberIndexById(senderId);
    if (idx < 0)
        return;

    if (kind == QLatin1String(Protocol::MediaKind::Audio)) {
        writeRemoteAudio(payload);
        return;
    }

    // video / screen：解码显示到对应成员宫格（共享时该宫格显示屏幕）
    QImage img = QImage::fromData(payload);
    if (img.isNull())
        return;
    if (VideoTile *t = m_grid->tileAt(idx)) {
        t->setCamOn(true); // 有画面即显示
        t->setVideoFrame(img);
    }

    // 已收到真实画面：停掉该成员的模拟源，避免模拟动画与真实画面交替闪烁
    if (idx < m_remoteSources.size()) {
        if (VideoSource *sim = m_remoteSources[idx]) {
            sim->stop();
            m_remoteSources[idx] = nullptr;
        }
        m_realVideoMembers.insert(senderId);
    }
}

void MainWindow::writeRemoteAudio(const QByteArray &pcm)
{
    if (!m_remoteAudioSinkDev)
        setupRemoteAudio();
    if (m_remoteAudioSinkDev)
        m_remoteAudioSinkDev->write(pcm);
}

void MainWindow::setupRemoteAudio()
{
    if (m_remoteAudioSink)
        return;
    QAudioFormat fmt;
    fmt.setSampleRate(44100);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);

    const QAudioDevice out = QMediaDevices::defaultAudioOutput();
    if (out.isNull())
        return;
    if (!out.isFormatSupported(fmt))
        fmt = out.preferredFormat(); // 设备不直接支持时退化为默认格式

    m_remoteAudioSink = new QAudioSink(out, fmt, this);
    m_remoteAudioSink->setBufferSize(20000);
    m_remoteAudioSinkDev = m_remoteAudioSink->start();
}

void MainWindow::teardownRemoteAudio()
{
    if (m_remoteAudioSink) {
        m_remoteAudioSink->stop();
        m_remoteAudioSink->deleteLater();
        m_remoteAudioSink = nullptr;
    }
    m_remoteAudioSinkDev = nullptr;
}

void MainWindow::startScreenSharing()
{
    m_sharingScreen = true;
    if (!m_shareTimer) {
        m_shareTimer = new QTimer(this);
        m_shareTimer->setInterval(250); // ~4fps，能看就行
        connect(m_shareTimer, &QTimer::timeout, this, &MainWindow::grabAndSendScreen);
    }
    m_shareTimer->start();
}

void MainWindow::stopScreenSharing()
{
    m_sharingScreen = false;
    if (m_shareTimer)
        m_shareTimer->stop();
}

int MainWindow::memberIndexById(const QString &id) const
{
    for (int i = 0; i < m_members.size(); ++i) {
        if (m_members[i].id == id)
            return i;
    }
    return -1;
}
