#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QObject>

class QAudioSource;
class QIODevice;

// 真实麦克风采集 + 音量检测。
// 基于 QAudioSource 从音频输入设备读取 PCM 数据，计算平滑后的 RMS 音量，
// 用滞回阈值（开始说话更高、持续说话更低）把音量转换为"是否在说话"状态，
// 用于驱动主持人宫格的绿色说话高亮与成员列表状态。
class AudioLevelSource : public QObject
{
    Q_OBJECT
public:
    explicit AudioLevelSource(QObject *parent = nullptr);
    ~AudioLevelSource() override;

    void start();
    void stop();
    bool isSpeaking() const { return m_speaking; }

    // 切换采集设备（若正在采集会自动重启）
    void setDevice(const QAudioDevice &device);
    QAudioDevice device() const { return m_deviceInfo; }

signals:
    void levelChanged(float level);      // 0.0 ~ 1.0 平滑后的 RMS
    void speakingChanged(bool speaking); // 说话状态变化
    void errorOccurred(const QString &message);
    void audioData(const QByteArray &data); // 原始 PCM（Int16，44.1kHz 单声道），供网络推流

private slots:
    void onReadyRead();

private:
    void updateState(bool speaking);

    QAudioSource *m_source = nullptr;
    QIODevice    *m_device = nullptr; // start() 返回的 PCM 读取设备
    QAudioDevice  m_deviceInfo;
    QAudioFormat  m_format;
    bool          m_speaking = false;
    float         m_smoothed = 0.0f;
    int           m_silentRuns = 0;
};
