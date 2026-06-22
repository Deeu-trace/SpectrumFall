#pragma once

#include <QVector>
#include <QString>
#include <QtGlobal>
#include <cmath>

/// 音频特效类型
enum class AudioEffectType { None, Echo, LowPass, HighPass, PitchShift };

/// 音频特效 DSP 处理器（支持流式分块处理，实时调节参数）
class AudioEffectProcessor
{
public:
    AudioEffectProcessor();

    /// 设置源 PCM 数据（单声道 float，-1.0~1.0）
    void setSource(const QVector<float>& pcm, int sampleRate);

    int sampleRate() const { return m_sampleRate; }

    /// ——— 效果类型 ———
    void setEffectType(AudioEffectType type);
    AudioEffectType effectType() const { return m_effectType; }

    /// ——— 回声 ———
    void setEchoDelayMs(float ms);
    void setEchoFeedback(float fb);
    void setEchoMix(float mix);
    float echoDelayMs() const { return m_echoDelayMs; }
    float echoFeedback() const { return m_echoFeedback; }
    float echoMix() const { return m_echoMix; }

    /// ——— 滤波器 ———
    void setFilterCutoffHz(float hz);
    void setFilterResonance(float q);
    float filterCutoffHz() const { return m_filterCutoffHz; }
    float filterResonance() const { return m_filterResonance; }

    /// ——— 变调 ———
    void setPitchSemitones(int st);
    int pitchSemitones() const { return m_pitchSemitones; }

    /// 批量处理（保留兼容）
    QVector<float> process();

    /// 流式分块处理（实时播放回调用）
    /// @param input  源 PCM 浮点数据
    /// @param output 输出 int16 数据
    /// @param nSamples 样本数
    void processChunk(const float* input, qint16* output, int nSamples);

    /// 重置所有内部状态（切换效果或 seek 时调用）
    void resetState();

    /// 写 WAV 文件
    bool writeToWav(const QString& path);

    QVector<float> getPreview(int seconds) const;

private:
    QVector<float> m_sourcePcm;
    int m_sampleRate = 44100;
    AudioEffectType m_effectType = AudioEffectType::None;

    float m_echoDelayMs = 300;
    float m_echoFeedback = 0.4f;
    float m_echoMix = 0.5f;

    float m_filterCutoffHz = 1000;
    float m_filterResonance = 0;

    int m_pitchSemitones = 0;

    // ——— 流式处理状态 ———
    // Echo 状态
    QVector<float> m_echoBuffer;
    int m_echoWritePos = 0;
    int m_echoDelaySamples = 0;

    // Filter 状态
    float m_lpfPrevOut = 0;

    // Pitch 状态（延迟线变调）
    QVector<float> m_pitchRingBuf;   ///< 环形缓冲区
    int m_pitchDelayWritePos = 0;    ///< 写入位置
    float m_pitchDelay = 0;          ///< 当前延迟量（0~MAX_DELAY 往返）

    // ——— DSP ———
    QVector<float> applyEcho(const QVector<float>& input);
    QVector<float> applyLowPass(const QVector<float>& input);
    QVector<float> applyHighPass(const QVector<float>& input);
    QVector<float> applyPitchShift(const QVector<float>& input);

    static bool writeWavFile(const QString& path, const QVector<float>& samples, int sr);
};
