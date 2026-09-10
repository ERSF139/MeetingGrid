#include "VideoSource.h"

#include <QCamera>
#include <QCameraDevice>
#include <QLinearGradient>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QPainter>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>

#include <QtMath>

// ===========================================================================
// SimulatedVideoSource
// ===========================================================================

SimulatedVideoSource::SimulatedVideoSource(const QString &label, const QColor &color,
                                           int fps, QObject *parent)
    : VideoSource(parent)
    , m_label(label)
    , m_color(color)
    , m_fps(qMax(1, fps))
    , m_timer(new QTimer(this))
{
    m_frame = QImage(480, 270, QImage::Format_RGB32);
    connect(m_timer, &QTimer::timeout, this, &SimulatedVideoSource::renderFrame);
    m_timer->setInterval(1000 / m_fps);
}

void SimulatedVideoSource::setFps(int fps)
{
    m_fps = qMax(1, fps);
    m_timer->setInterval(1000 / m_fps);
}

void SimulatedVideoSource::start()
{
    m_timer->start();
}

void SimulatedVideoSource::stop()
{
    m_timer->stop();
}

void SimulatedVideoSource::renderFrame()
{
    QPainter p(&m_frame);
    p.setRenderHint(QPainter::Antialiasing);
    const int W = m_frame.width();
    const int H = m_frame.height();
    const qreal phase = m_frameNo * 0.07;

    // 动态渐变背景
    QLinearGradient g(0, 0, W, H);
    g.setColorAt(0.0, m_color.lighter(125));
    g.setColorAt(0.55, m_color);
    g.setColorAt(1.0, m_color.darker(160));
    p.fillRect(m_frame.rect(), g);

    // 缓慢旋转的半透明圆
    const qreal cx = W / 2.0 + qCos(phase) * W * 0.30;
    const qreal cy = H / 2.0 + qSin(phase * 1.3) * H * 0.26;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 55));
    p.drawEllipse(QPointF(cx, cy), W * 0.13, W * 0.13);

    // 移动的斜向光带
    const qreal bandX = W * (0.5 + 0.5 * qSin(phase * 0.5));
    p.setPen(QPen(QColor(255, 255, 255, 35), W * 0.02));
    p.drawLine(QPointF(bandX, 0), QPointF(bandX + W * 0.25, H));

    // 四角装饰点（轻微闪烁感）
    p.setBrush(QColor(255, 255, 255, 70));
    for (int i = 0; i < 4; ++i) {
        const qreal blink = 0.5 + 0.5 * qSin(phase * 2 + i * 1.57);
        const qreal px = (i % 2 == 0) ? W * 0.08 : W * 0.92;
        const qreal py = (i / 2 == 0) ? H * 0.10 : H * 0.90;
        p.setBrush(QColor(255, 255, 255, int(40 + 50 * blink)));
        p.drawEllipse(QPointF(px, py), W * 0.02, W * 0.02);
    }

    // 名字水印（居中，半透明）
    QFont f = p.font();
    f.setPointSize(qMax(14, W / 16));
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(255, 255, 255, 150));
    p.drawText(m_frame.rect().adjusted(0, 0, 0, -H * 0.06), Qt::AlignCenter, m_label);

    ++m_frameNo;
    emit frameReady(m_frame);
}

// ===========================================================================
// CameraVideoSource
// ===========================================================================

CameraVideoSource::CameraVideoSource(const QCameraDevice &device, QObject *parent)
    : VideoSource(parent)
    , m_camera(new QCamera(device, this))
    , m_session(new QMediaCaptureSession(this))
    , m_sink(new QVideoSink(this))
{
    m_session->setCamera(m_camera);
    m_session->setVideoOutput(m_sink);

    connect(m_sink, &QVideoSink::videoFrameChanged, this, &CameraVideoSource::onFrame);
    connect(m_camera, &QCamera::errorOccurred, this,
            [this](QCamera::Error, const QString &errorString) {
                emit errorOccurred(errorString);
            });
}

QString CameraVideoSource::description() const
{
    return m_camera->cameraDevice().description();
}

void CameraVideoSource::start()
{
    m_camera->start();
}

void CameraVideoSource::stop()
{
    m_camera->stop();
}

void CameraVideoSource::onFrame(const QVideoFrame &frame)
{
    const QImage img = frame.toImage();
    if (!img.isNull())
        emit frameReady(img);
}
