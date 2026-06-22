#pragma once

#include <QObject>
#include <QVector>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QAudioSink>
#include <QIODevice>
#include <QElapsedTimer>

class AudioEffectProcessor;

/// 实时音频输出设备：将 PCM 数据通过 QAudioSink 推送到扬声器
class AudioOutputDevice : public QIODevice
{
    Q_OBJECT
public:
    explicit AudioOutputDevice(QObject* parent = nullptr);
    ~AudioOutputDevice() override = default;

    /// 设置源 PCM 数据和采样率
    void setSource(const float* data, qint64 totalSamples, int sampleRate);

    /// 设置效果处理器（nullptr = 直通）
    void setEffectProcessor(AudioEffectProcessor* proc) { m_effectProc = proc; }

    /// 跳转到指定采样位置
    void seekTo(qint64 samplePos);

    /// 获取当前播放采样位置（基于音频线程读取位置，可能不均匀）
    qint64 currentSample() const { return m_readPos; }

    /// 获取当前平滑播放位置（基于真实时间，用于 UI 同步）
    qint64 positionMs() const;

    /// 暂停/恢复内部时间钟（对应 QAudioSink suspend/resume）
    void pauseClock();
    void resumeClock();

    /// 总采样数
    qint64 totalSamples() const { return m_totalSamples; }

    /// 采样率
    int sampleRate() const { return m_sampleRate; }

    /// QIODevice 实现
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char*, qint64) override { return 0; }
    bool isSequential() const override { return false; }
    qint64 size() const override { return m_totalSamples * 2; }
    qint64 bytesAvailable() const override;
    // 不重写 seek() — QIODevice::open() 内部会调 seek(0)，重写会把 m_readPos 归零

private:
    const float* m_pcmData = nullptr;
    qint64 m_totalSamples = 0;
    qint64 m_readPos = 0;
    int m_sampleRate = 44100;
    AudioEffectProcessor* m_effectProc = nullptr;

    // 平滑位置：用真实时间驱动 UI，而不是 readPos（readPos 受音频线程唤醒间隔影响）
    mutable QElapsedTimer m_playClock;
    qint64 m_clockOffsetMs = 0;
    bool m_clockRunning = false;
};

/// 音频引擎：QMediaPlayer 负责播放，手写 WAV/MP3/FLAC 解析器获取 PCM 数据
class AudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine();

    /// 加载音频文件（WAV / MP3 / FLAC 格式），同时解析 PCM 和设置播放器
    bool loadFile(const QString& path);

    /// 播放
    void play();

    /// 暂停
    void pause();

    /// 跳转到指定位置（毫秒）
    void seek(qint64 ms);

    /// 设置音量（0.0 ~ 1.0）
    void setVolume(float vol);

    /// 设置播放速率
    void setPlaybackRate(qreal rate);

    /// 获取当前播放状态
    QMediaPlayer::PlaybackState state() const;

    /// 获取当前播放位置（毫秒）
    qint64 position() const;

    /// 获取音频总时长（毫秒）
    qint64 duration() const;

    /// 将处理后的音频写入临时 WAV 并切换播放源（保留兼容）
    bool loadProcessedAudio(const QVector<float>& processedPcm);

    /// 获取采样率
    int sampleRate() const;

    /// 获取声道数
    int channels() const;

    /// 获取 PCM 缓冲区（float 归一化，单声道）
    const QVector<float>& pcmData() const;

    /// 获取指定时间位置处的 PCM 窗口数据
    QVector<float> getWindowAt(qint64 ms, int windowSize) const;

    /// 是否已加载音频
    bool isLoaded() const;

    /// ── 实时特效播放 ──
    /// 启动基于 QAudioSink 的实时播放（支持外部效果器）
    void startRealtimePlayback(AudioEffectProcessor* effectProc = nullptr);
    /// 停止实时播放
    void stopRealtimePlayback();
    /// 是否正在实时播放（活跃且未暂停）
    bool isRealtimePlaying() const { return m_realtimeActive && m_realtimeSink && m_realtimeSink->state() != QAudio::SuspendedState; }

signals:
    void positionChanged(qint64 ms);
    void durationChanged(qint64 ms);
    void playbackStateChanged(QMediaPlayer::PlaybackState state);
    void loadComplete(bool success);

private slots:
    /// 在主线程中设置 QMediaPlayer 播放源（供跨线程 invokeMethod 调用）
    void applyPendingSource();
    /// 实时播放位置更新
    void onRealtimePositionUpdate();

private:
    QMediaPlayer* m_player;        ///< 音频播放器
    QAudioOutput* m_audioOutput;   ///< 音频输出
    QVector<float> m_pcmBuffer;    ///< PCM 数据缓冲（float 归一化，单声道）
    int m_sampleRate;              ///< 采样率
    int m_channels;                ///< 声道数
    int m_bitsPerSample;           ///< 位深度
    qint64 m_durationMs;           ///< 总时长（毫秒）
    bool m_loaded;                 ///< 是否已加载
    QUrl m_pendingSource;         ///< 待在主线程设置的播放源（跨线程安全）

    // 实时播放
    QAudioSink* m_realtimeSink = nullptr;
    AudioOutputDevice* m_realtimeDevice = nullptr;
    QTimer* m_realtimeTimer = nullptr;
    bool m_realtimeActive = false;

    /// 手写 WAV RIFF 解析器（支持 PCM 和 IEEE Float）
    bool parseWav(const QString& path);

    /// MP3 解码器（基于 dr_mp3）
    bool parseMp3(const QString& path);

    /// FLAC 解码器（基于 dr_flac）
    bool parseFlac(const QString& path);
};
