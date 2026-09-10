#pragma once

#include <QColor>
#include <QImage>
#include <QObject>

class QCamera;
class QCameraDevice;
class QMediaCaptureSession;
class QTimer;
class QVideoSink;
class QVideoFrame;

// 视频源抽象：会议中每个宫格对应一个视频源，统一以 QImage 帧对外输出。
class VideoSource : public QObject
{
    Q_OBJECT
public:
    explicit VideoSource(QObject *parent = nullptr) : QObject(parent) {}

    virtual void start() = 0;
    virtual void stop()  = 0;
    virtual bool isCamera() const { return false; }
    virtual QString description() const { return QString(); }

signals:
    void frameReady(const QImage &frame);
    void errorOccurred(const QString &message);
};

// 模拟视频源：以固定帧率绘制动态动画画面（渐变背景 + 移动元素 + 名字水印）。
// 用途：模拟远端成员画面；无摄像头时作为"主持人"画面的兜底。
class SimulatedVideoSource : public VideoSource
{
    Q_OBJECT
public:
    SimulatedVideoSource(const QString &label, const QColor &color, int fps = 15,
                         QObject *parent = nullptr);

    void start() override;
    void stop()  override;
    QString description() const override { return QStringLiteral("模拟画面"); }
    void setFps(int fps);

private slots:
    void renderFrame();

private:
    QImage  m_frame;
    QString m_label;
    QColor  m_color;
    QTimer *m_timer = nullptr;
    int     m_fps;
    int     m_frameNo = 0;
};

// 摄像头视频源：基于 QCamera + QMediaCaptureSession + QVideoSink 采集真实画面。
class CameraVideoSource : public VideoSource
{
    Q_OBJECT
public:
    explicit CameraVideoSource(const QCameraDevice &device, QObject *parent = nullptr);

    void start() override;
    void stop()  override;
    bool isCamera() const override { return true; }
    QString description() const override;

private slots:
    void onFrame(const QVideoFrame &frame);

private:
    QCamera               *m_camera  = nullptr;
    QMediaCaptureSession  *m_session = nullptr;
    QVideoSink            *m_sink    = nullptr;
};
