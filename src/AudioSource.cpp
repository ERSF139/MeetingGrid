#include "AudioSource.h"

#include <QAudio>
#include <QAudioSource>
#include <QDebug>
#include <QIODevice>
#include <QMediaDevices>
#include <QtMath>

namespace {

// 读取 PCM(Int16) 缓冲并计算归一化 RMS（0.0 ~ 1.0，1.0 = 满幅）
float computeRms(const QByteArray &data)
{
    if (data.size() < 2)
        return 0.0f;
    const qint16 *samples = reinterpret_cast<const qint16 *>(data.constData());
    const int count = data.size() / 2;
    qint64 sum = 0;
    for (int i = 0; i < count; ++i) {
        const qint32 s = samples[i];
        sum += qint64(s) * s;
    }
    return static_cast<float>(qSqrt(qreal(sum) / count) / 32768.0);
}

} // namespace

AudioLevelSource::AudioLevelSource(QObject *parent)
    : QObject(parent)
    , m_deviceInfo(QMediaDevices::defaultAudioInput())
{
    // 44.1kHz 单声道 Int16：通用格式，绝大多数设备直接支持
    m_format.setSampleRate(44100);
    m_format.setChannelCount(1);
    m_format.setSampleFormat(QAudioFormat::Int16);
}

AudioLevelSource::~AudioLevelSource()
{
    stop();
}

void AudioLevelSource::start()
{
    if (m_source)
        return;

    if (m_deviceInfo.isNull()) {
        emit errorOccurred(QStringLiteral("未检测到音频输入设备"));
        return;
    }
    if (!m_format.isValid()) {
        emit errorOccurred(QStringLiteral("音频格式无效"));
        return;
    }

    m_source = new QAudioSource(m_deviceInfo, m_format, this);
    m_device = m_source->start();
    if (!m_device) {
        emit errorOccurred(QStringLiteral("无法打开音频输入，请检查麦克风权限"));
        delete m_source;
        m_source = nullptr;
        return;
    }

    connect(m_device, &QIODevice::readyRead, this, &AudioLevelSource::onReadyRead);
    connect(m_source, &QAudioSource::stateChanged, this, [this](QAudio::State state) {
        // 运行中突然进入 StoppedState 视为设备错误（权限被撤 / 设备被拔）
        if (state == QAudio::StoppedState && m_source
            && m_source->error() != QAudio::NoError) {
            emit errorOccurred(QStringLiteral("音频输入出错，已停止采集"));
            stop();
        }
    });
}

void AudioLevelSource::stop()
{
    if (m_source) {
        m_source->stop();
        m_source->deleteLater();
        m_source = nullptr;
    }
    m_device = nullptr;
    updateState(false);
    m_smoothed = 0.0f;
    m_silentRuns = 0;
}

void AudioLevelSource::setDevice(const QAudioDevice &device)
{
    if (device == m_deviceInfo)
        return;
    const bool running = (m_source != nullptr);
    stop();
    m_deviceInfo = device;
    if (running)
        start();
}

void AudioLevelSource::onReadyRead()
{
    if (!m_device)
        return;
    const QByteArray data = m_device->readAll();
    const float level = computeRms(data);

    // 平滑：短暂尖峰不会误触发，持续音量才判定
    m_smoothed = m_smoothed * 0.7f + level * 0.3f;

    // 滞回阈值：开始说话要求更高，持续说话用更低阈值保持，避免边界抖动
    constexpr float kSpeakStart = 0.018f;
    constexpr float kSpeakHold  = 0.010f;
    if (m_speaking) {
        if (m_smoothed < kSpeakHold) {
            if (++m_silentRuns >= 8) // 连续静音才判定停止说话
                updateState(false);
        } else {
            m_silentRuns = 0;
        }
    } else if (m_smoothed > kSpeakStart) {
        m_silentRuns = 0;
        updateState(true);
    }

    emit levelChanged(m_smoothed);
    emit audioData(data);
}

void AudioLevelSource::updateState(bool speaking)
{
    if (m_speaking == speaking)
        return;
    m_speaking = speaking;
    emit speakingChanged(speaking);
}
