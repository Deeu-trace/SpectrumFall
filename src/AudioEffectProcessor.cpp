#include "AudioEffectProcessor.h"
#include <QFile>
#include <QDataStream>
#include <algorithm>
#include <cstring>

AudioEffectProcessor::AudioEffectProcessor()
    : m_sampleRate(44100)
    , m_effectType(AudioEffectType::None)
    , m_echoDelayMs(300.0f)
    , m_echoFeedback(0.4f)
    , m_echoMix(0.5f)
    , m_filterCutoffHz(1000.0f)
    , m_filterResonance(0.0f)
    , m_pitchSemitones(0)
{
}

void AudioEffectProcessor::setSource(const QVector<float>& pcm, int sampleRate)
{
    m_sourcePcm = pcm;
    m_sampleRate = sampleRate;
}

void AudioEffectProcessor::setEffectType(AudioEffectType type)
{
    if (m_effectType != type) {
        m_effectType = type;
        resetState();
    }
}

void AudioEffectProcessor::setEchoDelayMs(float ms)
{
    m_echoDelayMs = qBound(20.0f, ms, 2000.0f);
}

void AudioEffectProcessor::setEchoFeedback(float fb)
{
    m_echoFeedback = qBound(0.0f, fb, 0.95f);
}

void AudioEffectProcessor::setEchoMix(float mix)
{
    m_echoMix = qBound(0.0f, mix, 1.0f);
}

void AudioEffectProcessor::setFilterCutoffHz(float hz)
{
    m_filterCutoffHz = qBound(20.0f, hz, static_cast<float>(m_sampleRate / 2 - 1));
}

void AudioEffectProcessor::setFilterResonance(float q)
{
    m_filterResonance = qBound(0.0f, q, 0.95f);
}

void AudioEffectProcessor::setPitchSemitones(int st)
{
    m_pitchSemitones = qBound(-12, st, 12);
}

QVector<float> AudioEffectProcessor::process()
{
    if (m_sourcePcm.isEmpty()) return {};

    switch (m_effectType) {
    case AudioEffectType::Echo:      return applyEcho(m_sourcePcm);
    case AudioEffectType::LowPass:   return applyLowPass(m_sourcePcm);
    case AudioEffectType::HighPass:  return applyHighPass(m_sourcePcm);
    case AudioEffectType::PitchShift:return applyPitchShift(m_sourcePcm);
    default:                         return m_sourcePcm;
    }
}

QVector<float> AudioEffectProcessor::getPreview(int seconds) const
{
    if (m_sourcePcm.isEmpty()) return {};
    int samples = qMin(seconds * m_sampleRate, m_sourcePcm.size());
    return m_sourcePcm.mid(0, samples);
}

// ═══════════════════════════════════════════════════════════════
//  流式分块处理（实时播放核心）
// ═══════════════════════════════════════════════════════════════

void AudioEffectProcessor::resetState()
{
    m_echoBuffer.clear();
    m_echoWritePos = 0;
    m_echoDelaySamples = 0;
    m_lpfPrevOut = 0;
    m_pitchRingBuf.clear();
    m_pitchDelayWritePos = 0;
    m_pitchDelay = 1024.0f;  // 半程延迟
}

void AudioEffectProcessor::processChunk(const float* input, qint16* output, int nSamples)
{
    if (!input || !output || nSamples <= 0) return;

    switch (m_effectType) {
    case AudioEffectType::None:
        for (int i = 0; i < nSamples; ++i)
            output[i] = static_cast<qint16>(qBound(-1.0f, input[i], 1.0f) * 32767.0f);
        break;

    case AudioEffectType::Echo: {
        int delaySamp = static_cast<int>(m_echoDelayMs * m_sampleRate / 1000.0f);
        if (delaySamp < 1) delaySamp = 1;

        // 平滑调整延迟：只 resize，不清零（避免爆音）
        if (delaySamp != m_echoDelaySamples) {
            int oldSize = m_echoBuffer.size();
            m_echoBuffer.resize(delaySamp + 1);
            if (delaySamp > m_echoDelaySamples) {
                // 扩容：新区域填零；旧数据保持在原位置继续用
                for (int j = oldSize; j < m_echoBuffer.size(); ++j)
                    m_echoBuffer[j] = 0.0f;
            }
            if (m_echoWritePos >= delaySamp + 1) m_echoWritePos = 0;
            m_echoDelaySamples = delaySamp;
        }
        for (int i = 0; i < nSamples; ++i) {
            int readPos = (m_echoWritePos + 1) % m_echoDelaySamples;
            float delayed = m_echoBuffer[readPos];
            float out = input[i] + m_echoMix * delayed;
            m_echoBuffer[m_echoWritePos] = input[i] + m_echoFeedback * delayed;
            m_echoWritePos = readPos;
            // prev: tanh 软限幅，无断点
            out = std::tanhf(out);
            output[i] = static_cast<qint16>(out * 32767.0f);
        }
        break;
    }

    case AudioEffectType::LowPass: {
        float alpha = 1.0f - std::exp(-6.2831853f * m_filterCutoffHz / m_sampleRate);
        for (int i = 0; i < nSamples; ++i) {
            float filtered = m_lpfPrevOut + alpha * (input[i] - m_lpfPrevOut);
            if (m_filterResonance > 0.001f)
                filtered += m_filterResonance * (filtered - m_lpfPrevOut);
            m_lpfPrevOut = filtered;
            output[i] = static_cast<qint16>(qBound(-1.0f, filtered, 1.0f) * 32767.0f);
        }
        break;
    }

    case AudioEffectType::HighPass: {
        float alpha = 1.0f - std::exp(-6.2831853f * m_filterCutoffHz / m_sampleRate);
        for (int i = 0; i < nSamples; ++i) {
            float lpf = m_lpfPrevOut + alpha * (input[i] - m_lpfPrevOut);
            if (m_filterResonance > 0.001f)
                lpf += m_filterResonance * (lpf - m_lpfPrevOut);
            m_lpfPrevOut = lpf;
            float hpf = input[i] - lpf;
            output[i] = static_cast<qint16>(qBound(-1.0f, hpf, 1.0f) * 32767.0f);
        }
        break;
    }

    case AudioEffectType::PitchShift: {
        // ── 延迟线变调（Delay-Line Pitch Shifter）──
        // 原理：延迟量以 (1-ratio)/sample 的速率线性变化，
        //   升调时延迟递减，降调时延迟递增。
        //   延迟往返到边界时自动翻转，用两个间隔半周期的
        //   读指针 + Hann 窗交叉淡变消除跳变噪音。
        const int BUF_SIZE = 4096;
        const int MAX_DELAY = 2048;
        const float TWO_PI = 6.2831853f;

        if (m_pitchRingBuf.size() != BUF_SIZE) {
            m_pitchRingBuf.resize(BUF_SIZE);
            m_pitchRingBuf.fill(0.0f);
            m_pitchDelayWritePos = 0;
            m_pitchDelay = MAX_DELAY * 0.5f;
        }

        float ratio = std::pow(2.0f, m_pitchSemitones / 12.0f);
        float delayRate = 1.0f - ratio;  // 升调<0, 降调>0

        for (int i = 0; i < nSamples; ++i) {
            // 写入
            m_pitchRingBuf[m_pitchDelayWritePos] = input[i];
            m_pitchDelayWritePos = (m_pitchDelayWritePos + 1) % BUF_SIZE;

            // 读指针1：当前延迟
            float rp1 = m_pitchDelayWritePos - m_pitchDelay;
            while (rp1 < 0) rp1 += BUF_SIZE;
            int i1 = static_cast<int>(rp1) % BUF_SIZE;
            float f1 = rp1 - floorf(rp1);
            float s1 = (1.0f - f1) * m_pitchRingBuf[i1]
                       + f1 * m_pitchRingBuf[(i1 + 1) % BUF_SIZE];

            // 读指针2：延迟偏移 MAX_DELAY/2，独立取模保证连续性
            float d2 = m_pitchDelay + MAX_DELAY * 0.5f;
            if (d2 >= MAX_DELAY) d2 -= MAX_DELAY;
            float rp2 = m_pitchDelayWritePos - d2;
            while (rp2 < 0) rp2 += BUF_SIZE;
            int i2 = static_cast<int>(rp2) % BUF_SIZE;
            float f2 = rp2 - floorf(rp2);
            float s2 = (1.0f - f2) * m_pitchRingBuf[i2]
                       + f2 * m_pitchRingBuf[(i2 + 1) % BUF_SIZE];

            // Hann 交叉淡变
            float phase = m_pitchDelay / MAX_DELAY;
            float w1 = 0.5f * (1.0f - cosf(TWO_PI * phase));
            float w2 = 0.5f * (1.0f - cosf(TWO_PI * fmodf(phase + 0.5f, 1.0f)));

            float s = s1 * w1 + s2 * w2;
            output[i] = static_cast<qint16>(qBound(-1.0f, s, 1.0f) * 32767.0f);

            // 推进延迟
            m_pitchDelay += delayRate;
            if (m_pitchDelay < 0) m_pitchDelay += MAX_DELAY;
            if (m_pitchDelay >= MAX_DELAY) m_pitchDelay -= MAX_DELAY;
        }
        break;
    }
    }
}

// ═══════════════════════════════════════════════════════════════
//  回声效果
//  算法: y[n] = x[n] + mix * buffer[n - delay]
//        buffer[n] = x[n] + feedback * buffer[n - delay]
// ═══════════════════════════════════════════════════════════════
QVector<float> AudioEffectProcessor::applyEcho(const QVector<float>& input)
{
    int delaySamples = static_cast<int>(m_echoDelayMs * m_sampleRate / 1000.0f);
    if (delaySamples <= 0) return input;

    QVector<float> output(input.size());
    QVector<float> delayBuffer(delaySamples, 0.0f);
    int writePos = 0;

    for (int i = 0; i < input.size(); ++i) {
        int readPos = (writePos + 1) % delaySamples;
        float delayed = delayBuffer[readPos];

        // 输出 = 干信号 + 混合后的延迟信号
        output[i] = input[i] + m_echoMix * delayed;

        // 更新延迟缓冲（含反馈）
        delayBuffer[writePos] = input[i] + m_echoFeedback * delayed;

        writePos = readPos;
    }

    // tanh 软限幅防止爆音
    for (int i = 0; i < output.size(); ++i) {
        output[i] = std::tanhf(output[i]);
    }

    return output;
}

// ═══════════════════════════════════════════════════════════════
//  低通滤波器（1-pole IIR）
//  算法: y[n] = y[n-1] + alpha * (x[n] - y[n-1])
//        alpha = 1 - exp(-2*PI*fc/fs)
//  共振: 通过双二次结构增加峰值（简化: feedback = resonance * (y[n] - lastSample)）
// ═══════════════════════════════════════════════════════════════
QVector<float> AudioEffectProcessor::applyLowPass(const QVector<float>& input)
{
    QVector<float> output(input.size());
    float alpha = 1.0f - std::exp(-6.2831853f * m_filterCutoffHz / m_sampleRate);
    float prevOut = 0.0f;

    for (int i = 0; i < input.size(); ++i) {
        // 1-pole LPF
        float filtered = prevOut + alpha * (input[i] - prevOut);

        // 共振增强
        if (m_filterResonance > 0.001f) {
            float resonance = m_filterResonance * (filtered - prevOut);
            filtered += resonance;
        }

        prevOut = filtered;
        output[i] = filtered;
    }

    return output;
}

// ═══════════════════════════════════════════════════════════════
//  高通滤波器 = 原信号 - 低通滤波后信号
//  共振: 低通阶段的共振经过减法后产生互补峰值
// ═══════════════════════════════════════════════════════════════
QVector<float> AudioEffectProcessor::applyHighPass(const QVector<float>& input)
{
    // 先生成低通版本
    QVector<float> lowPassed = applyLowPass(input);
    QVector<float> output(input.size());

    for (int i = 0; i < input.size(); ++i) {
        output[i] = input[i] - lowPassed[i];
    }

    return output;
}

// ═══════════════════════════════════════════════════════════════
//  变调（不变速）
//  算法: 以改变的速率读取输入（比例 = 2^(semitones/12)）
//        使用线性插值保持平滑
//        输出长度 = 输入长度（保持时长不变）
// ═══════════════════════════════════════════════════════════════
QVector<float> AudioEffectProcessor::applyPitchShift(const QVector<float>& input)
{
    float ratio = std::pow(2.0f, m_pitchSemitones / 12.0f);
    QVector<float> output(input.size());
    float readPos = 0.0f;

    for (int i = 0; i < output.size(); ++i) {
        int idx = static_cast<int>(readPos);
        float frac = readPos - idx;

        if (idx + 1 < input.size() && idx >= 0) {
            output[i] = (1.0f - frac) * input[idx] + frac * input[idx + 1];
        } else if (idx < input.size() && idx >= 0) {
            output[i] = input[idx];
        } else {
            output[i] = 0.0f;
        }

        readPos += ratio;
        if (readPos >= input.size()) {
            readPos -= input.size();
        }
        if (readPos < 0) {
            readPos += input.size();
        }
    }

    return output;
}

// ═══════════════════════════════════════════════════════════════
//  WAV 文件写入（RIFF/WAVE 格式，32-bit float PCM）
// ═══════════════════════════════════════════════════════════════
bool AudioEffectProcessor::writeToWav(const QString& path)
{
    QVector<float> processed = process();
    if (processed.isEmpty()) return false;
    return writeWavFile(path, processed, m_sampleRate);
}

bool AudioEffectProcessor::writeWavFile(const QString& path, const QVector<float>& samples, int sr)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);

    int numSamples = samples.size();
    int dataSize = numSamples * 2;  // 16-bit PCM
    int fileSize = 36 + dataSize;

    // RIFF header
    ds.writeRawData("RIFF", 4);
    ds << fileSize;
    ds.writeRawData("WAVE", 4);

    // fmt chunk
    ds.writeRawData("fmt ", 4);
    ds << static_cast<qint32>(16);         // chunk size
    ds << static_cast<qint16>(1);          // PCM format
    ds << static_cast<qint16>(1);          // mono
    ds << static_cast<qint32>(sr);         // sample rate
    ds << static_cast<qint32>(sr * 2);     // byte rate
    ds << static_cast<qint16>(2);          // block align
    ds << static_cast<qint16>(16);         // bits per sample

    // data chunk
    ds.writeRawData("data", 4);
    ds << dataSize;

    // PCM samples (float → int16)
    for (int i = 0; i < numSamples; ++i) {
        float val = qBound(-1.0f, samples[i], 1.0f);
        qint16 s = static_cast<qint16>(val * 32767.0f);
        ds << s;
    }

    file.close();
    return true;
}
