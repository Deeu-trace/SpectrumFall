#include "AudioEngine.h"
#include <QFile>
#include <QUrl>
#include <QDebug>
#include <QThread>
#include <cstring>
#include <QDataStream>

// dr_mp3 实现（只在一个 .cpp 中定义）
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "dr_mp3.h"

AudioEngine::AudioEngine(QObject* parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_sampleRate(44100)
    , m_channels(1)
    , m_bitsPerSample(16)
    , m_durationMs(0)
    , m_loaded(false)
{
    m_player->setAudioOutput(m_audioOutput);

    // 转发播放器信号
    connect(m_player, &QMediaPlayer::positionChanged, this, &AudioEngine::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &AudioEngine::durationChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &AudioEngine::playbackStateChanged);
}

AudioEngine::~AudioEngine()
{
}

bool AudioEngine::loadFile(const QString& path)
{
    m_loaded = false;
    m_pcmBuffer.clear();
    m_durationMs = 0;

    bool success = false;

    // 根据扩展名分发到不同解析器
    if (path.endsWith(".mp3", Qt::CaseInsensitive)) {
        success = parseMp3(path);
    } else {
        // 默认尝试 WAV 解析
        success = parseWav(path);
    }

    if (!success) {
        emit loadComplete(false);
        return false;
    }

    // 设置 QMediaPlayer 播放源
    const QUrl sourceUrl = QUrl::fromLocalFile(path);
    if (QThread::currentThread() == thread()) {
        m_player->setSource(sourceUrl);
    } else {
        QMetaObject::invokeMethod(m_player, [this, sourceUrl]() {
            m_player->setSource(sourceUrl);
        }, Qt::BlockingQueuedConnection);
    }
    m_loaded = true;
    emit loadComplete(true);
    return true;
}

void AudioEngine::play()
{
    m_player->play();
}

void AudioEngine::pause()
{
    m_player->pause();
}

void AudioEngine::seek(qint64 ms)
{
    m_player->setPosition(ms);
}

void AudioEngine::setVolume(float vol)
{
    m_audioOutput->setVolume(qBound(0.0f, vol, 1.0f));
}

void AudioEngine::setPlaybackRate(qreal rate)
{
    m_player->setPlaybackRate(rate);
}

QMediaPlayer::PlaybackState AudioEngine::state() const
{
    return m_player->playbackState();
}

qint64 AudioEngine::position() const
{
    return m_player->position();
}

qint64 AudioEngine::duration() const
{
    return m_player->duration();
}

int AudioEngine::sampleRate() const
{
    return m_sampleRate;
}

int AudioEngine::channels() const
{
    return m_channels;
}

const QVector<float>& AudioEngine::pcmData() const
{
    return m_pcmBuffer;
}

QVector<float> AudioEngine::getWindowAt(qint64 ms, int windowSize) const
{
    QVector<float> window(windowSize, 0.0f);
    if (m_pcmBuffer.isEmpty() || m_sampleRate <= 0) {
        return window;
    }

    // 计算起始采样索引
    qint64 startSample = ms * m_sampleRate / 1000;
    qint64 totalSamples = m_pcmBuffer.size();

    for (int i = 0; i < windowSize; ++i) {
        qint64 idx = startSample - windowSize / 2 + i;
        if (idx >= 0 && idx < totalSamples) {
            window[i] = m_pcmBuffer[static_cast<int>(idx)];
        }
    }

    return window;
}

bool AudioEngine::isLoaded() const
{
    return m_loaded;
}

bool AudioEngine::parseWav(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "AudioEngine: cannot open file:" << path;
        return false;
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setFloatingPointPrecision(QDataStream::SinglePrecision);

    auto fail = [&](const QString& msg) {
        qWarning() << "AudioEngine:" << msg << path;
        return false;
    };

    auto readBytes = [&](char* dst, qint64 len) -> bool {
        return in.readRawData(dst, len) == len;
    };

    // RIFF header
    char riffId[4] = {};
    if (!readBytes(riffId, 4) || std::memcmp(riffId, "RIFF", 4) != 0) {
        return fail("not a RIFF file:");
    }

    quint32 fileSize = 0;
    if (!(in >> fileSize)) {
        return fail("truncated RIFF header:");
    }

    char waveId[4] = {};
    if (!readBytes(waveId, 4) || std::memcmp(waveId, "WAVE", 4) != 0) {
        return fail("not a WAVE file:");
    }

    bool foundFmt = false;
    bool foundData = false;
    quint32 dataSize = 0;
    quint16 audioFormat = 0;
    quint16 numChannels = 0;
    quint32 sampleRate = 0;
    quint16 bitsPerSample = 0;

    while (!in.atEnd()) {
        char chunkId[4] = {};
        if (!readBytes(chunkId, 4)) {
            break;
        }

        quint32 chunkSize = 0;
        if (!(in >> chunkSize)) {
            return fail("truncated chunk header:");
        }

        const QString chunkName = QString::fromLatin1(chunkId, 4);

        if (chunkName == QLatin1String("fmt ")) {
            if (chunkSize < 16) {
                return fail(QStringLiteral("invalid fmt chunk size:"));
            }

            quint32 byteRate = 0;
            quint16 blockAlign = 0;
            quint16 effectiveAudioFormat = audioFormat;

            if (!(in >> audioFormat
                    >> numChannels
                    >> sampleRate
                    >> byteRate
                    >> blockAlign
                    >> bitsPerSample)) {
                return fail("truncated fmt chunk:");
            }

            const qint64 extraSize = static_cast<qint64>(chunkSize) - 16;
            QByteArray extraData;
            if (extraSize > 0) {
                extraData = file.read(extraSize);
                if (extraData.size() != extraSize) {
                    return fail("truncated fmt extra data:");
                }
            }

            if (audioFormat == 65534) {
                if (extraData.size() < 24) {
                    return fail("invalid extensible fmt chunk:");
                }

                static const unsigned char pcmSubtype[16] = {
                    0x01, 0x00, 0x00, 0x00,
                    0x00, 0x00,
                    0x10, 0x00,
                    0x80, 0x00,
                    0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
                };
                static const unsigned char floatSubtype[16] = {
                    0x03, 0x00, 0x00, 0x00,
                    0x00, 0x00,
                    0x10, 0x00,
                    0x80, 0x00,
                    0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
                };

                const unsigned char* subFormat = reinterpret_cast<const unsigned char*>(extraData.constData() + 8);
                if (std::memcmp(subFormat, pcmSubtype, 16) == 0) {
                    effectiveAudioFormat = 1;
                } else if (std::memcmp(subFormat, floatSubtype, 16) == 0) {
                    effectiveAudioFormat = 3;
                } else {
                    qWarning() << "AudioEngine: unsupported extensible WAV subformat in:" << path;
                    return false;
                }
            } else if (audioFormat != 1 && audioFormat != 3) {
                qWarning() << "AudioEngine: unsupported audio format:" << audioFormat
                           << "(only PCM=1, IEEE Float=3 and extensible WAV are supported)";
                return false;
            }

            audioFormat = effectiveAudioFormat;

            m_channels = numChannels;
            m_sampleRate = static_cast<int>(sampleRate);
            m_bitsPerSample = bitsPerSample;
            foundFmt = true;
        } else if (chunkName == QLatin1String("data")) {
            dataSize = chunkSize;
            foundData = true;
            break;
        } else {
            if (in.skipRawData(static_cast<qint64>(chunkSize)) != static_cast<qint64>(chunkSize)) {
                return fail(QStringLiteral("truncated chunk: %1").arg(chunkName));
            }
        }

        if (chunkSize & 1) {
            char padding = 0;
            if (!readBytes(&padding, 1)) {
                return fail("truncated chunk padding:");
            }
        }
    }

    if (!foundFmt || !foundData) {
        qWarning() << "AudioEngine: missing fmt or data chunk in:" << path
                   << "foundFmt=" << foundFmt << "foundData=" << foundData;
        return false;
    }

    int bytesPerSample = m_bitsPerSample / 8;
    if (bytesPerSample <= 0 || m_channels <= 0) {
        qWarning() << "AudioEngine: invalid format - bitsPerSample:" << m_bitsPerSample
                   << "channels:" << m_channels;
        return false;
    }

    if (m_sampleRate <= 0) {
        qWarning() << "AudioEngine: invalid sample rate:" << m_sampleRate << "in" << path;
        return false;
    }

    const qint64 bytesPerFrame = static_cast<qint64>(bytesPerSample) * m_channels;
    if (bytesPerFrame <= 0) {
        return fail("invalid bytes per frame:");
    }

    const qint64 availableBytes = qMin<qint64>(dataSize, file.size() - file.pos());
    if (availableBytes <= 0) {
        return fail("empty data chunk:");
    }

    QByteArray rawData = file.read(availableBytes);
    if (rawData.size() != availableBytes) {
        qWarning() << "AudioEngine: truncated data chunk:" << path
                   << "expected=" << availableBytes
                   << "actual=" << rawData.size();
    }

    int totalFrames = static_cast<int>(rawData.size() / bytesPerFrame);
    m_pcmBuffer.resize(totalFrames);

    const char* data = rawData.constData();

    if (audioFormat == 1) {
        for (int i = 0; i < totalFrames; ++i) {
            float sample = 0.0f;
            for (int ch = 0; ch < m_channels; ++ch) {
                const int idx = (i * m_channels + ch) * bytesPerSample;
                if (idx + bytesPerSample > rawData.size()) {
                    break;
                }

                float channelSample = 0.0f;
                if (m_bitsPerSample == 16) {
                    qint16 val = 0;
                    std::memcpy(&val, data + idx, 2);
                    channelSample = val / 32768.0f;
                } else if (m_bitsPerSample == 8) {
                    quint8 val = 0;
                    std::memcpy(&val, data + idx, 1);
                    channelSample = (static_cast<float>(val) - 128.0f) / 128.0f;
                } else if (m_bitsPerSample == 24) {
                    unsigned char b[3];
                    std::memcpy(b, data + idx, 3);
                    int val = (b[2] << 16) | (b[1] << 8) | b[0];
                    if (val & 0x800000) val |= 0xFF000000;
                    channelSample = static_cast<float>(val) / 8388608.0f;
                } else if (m_bitsPerSample == 32) {
                    qint32 val = 0;
                    std::memcpy(&val, data + idx, 4);
                    channelSample = val / 2147483648.0f;
                }
                sample += channelSample;
            }
            m_pcmBuffer[i] = sample / m_channels;
        }
    } else if (audioFormat == 3) {
        if (m_bitsPerSample != 32) {
            qWarning() << "AudioEngine: IEEE Float only supports 32-bit, got:" << m_bitsPerSample;
            return false;
        }

        for (int i = 0; i < totalFrames; ++i) {
            float sample = 0.0f;
            for (int ch = 0; ch < m_channels; ++ch) {
                const int idx = (i * m_channels + ch) * 4;
                if (idx + 4 > rawData.size()) {
                    break;
                }
                float channelSample = 0.0f;
                std::memcpy(&channelSample, data + idx, 4);
                sample += channelSample;
            }
            m_pcmBuffer[i] = sample / m_channels;
        }
    }

    m_durationMs = static_cast<qint64>(totalFrames) * 1000 / m_sampleRate;

    qDebug() << "AudioEngine: loaded" << path
             << "format:" << (audioFormat == 1 ? "PCM" : "IEEE Float")
             << m_sampleRate << "Hz" << m_channels << "ch"
             << m_bitsPerSample << "bit"
             << "duration:" << m_durationMs << "ms"
             << "frames:" << totalFrames;

    return true;
}

bool AudioEngine::parseMp3(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "AudioEngine: cannot open MP3 file:" << path;
        return false;
    }

    QByteArray fileData = file.readAll();
    if (fileData.isEmpty()) {
        qWarning() << "AudioEngine: MP3 file is empty:" << path;
        return false;
    }

    // dr_mp3 解码
    drmp3_config config;
    drmp3_uint64 totalFrames = 0;

    float* pPCM = drmp3_open_memory_and_read_pcm_frames_f32(
        fileData.constData(), fileData.size(),
        &config, &totalFrames, nullptr);

    if (!pPCM || totalFrames == 0) {
        qWarning() << "AudioEngine: failed to decode MP3:" << path;
        if (pPCM) drmp3_free(pPCM, nullptr);
        return false;
    }

    m_sampleRate = static_cast<int>(config.sampleRate);
    m_channels = static_cast<int>(config.channels);
    m_bitsPerSample = 32; // dr_mp3 输出 32-bit float

    // dr_mp3 输出交错 float（多声道），转为单声道均值
    m_pcmBuffer.resize(static_cast<int>(totalFrames));
    for (drmp3_uint64 i = 0; i < totalFrames; ++i) {
        float sample = 0.0f;
        for (int ch = 0; ch < m_channels; ++ch) {
            sample += pPCM[i * m_channels + ch];
        }
        m_pcmBuffer[static_cast<int>(i)] = sample / m_channels;
    }

    drmp3_free(pPCM, nullptr);

    // 计算总时长
    m_durationMs = static_cast<qint64>(totalFrames) * 1000 / m_sampleRate;

    qDebug() << "AudioEngine: loaded MP3" << path
             << m_sampleRate << "Hz" << m_channels << "ch"
             << "duration:" << m_durationMs << "ms"
             << "frames:" << totalFrames;

    return true;
}
