#pragma once

#include <QObject>
#include <QVector>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QAudioOutput>

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

signals:
    void positionChanged(qint64 ms);
    void durationChanged(qint64 ms);
    void playbackStateChanged(QMediaPlayer::PlaybackState state);
    void loadComplete(bool success);

private slots:
    /// 在主线程中设置 QMediaPlayer 播放源（供跨线程 invokeMethod 调用）
    void applyPendingSource();

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

    /// 手写 WAV RIFF 解析器（支持 PCM 和 IEEE Float）
    bool parseWav(const QString& path);

    /// MP3 解码器（基于 dr_mp3）
    bool parseMp3(const QString& path);

    /// FLAC 解码器（基于 dr_flac）
    bool parseFlac(const QString& path);
};
