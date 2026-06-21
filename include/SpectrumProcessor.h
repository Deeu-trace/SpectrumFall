#pragma once

#include <QVector>
#include <cmath>

/// 频谱后处理工具：将原始线性 FFT 幅度转换为视觉友好的柱状值
/// 三项处理：对数频率轴映射 + 频带能量均值 + 幂律压缩
namespace SpectrumProcessor {

/// @param magnitude  原始 FFT 幅度谱（归一化 [0,1]，共 fftSize/2 个 bin）
/// @param sampleRate 音频采样率（Hz）
/// @param fftSize    FFT 窗口大小
/// @param barCount   输出柱数
/// @param compressionPower 压缩强度（0.1=很平, 1.0=不压缩, 默认 0.33）
/// @param minFreq    最小显示频率（Hz），默认 30
/// @param maxFreq    最大显示频率（Hz），默认 18000
/// @return 处理后的柱值（[0,1]，视觉均匀分布）
inline QVector<float> process(
    const QVector<float>& magnitude,
    int sampleRate,
    int fftSize,
    int barCount,
    float compressionPower = 0.33f,
    float minFreq = 30.0f,
    float maxFreq = 18000.0f)
{
    QVector<float> bars(barCount, 0.0f);
    if (magnitude.isEmpty() || barCount <= 0 || sampleRate <= 0 || fftSize <= 0) {
        return bars;
    }

    const int numBins = magnitude.size();
    const float binWidth = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

    // ── 1. 对数频率轴：每根柱子覆盖的频率范围按 log10 等分 ──
    const float logMin = std::log10(qMax(minFreq, binWidth));
    const float logMax = std::log10(qMin(maxFreq, sampleRate * 0.5f));
    const float logRange = logMax - logMin;

    if (logRange <= 0.0f) return bars;

    // ── 2. 计算每根柱子的频带范围 + 频带能量均值 ──
    for (int i = 0; i < barCount; ++i) {
        const float logFreqLow  = logMin + logRange * static_cast<float>(i) / static_cast<float>(barCount);
        const float logFreqHigh = logMin + logRange * static_cast<float>(i + 1) / static_cast<float>(barCount);
        const float freqLow  = std::pow(10.0f, logFreqLow);
        const float freqHigh = std::pow(10.0f, logFreqHigh);

        int binStart = static_cast<int>(freqLow / binWidth);
        int binEnd   = static_cast<int>(freqHigh / binWidth);

        // 钳制到有效范围，确保每柱至少 1 个 bin
        binStart = qMax(0, qMin(binStart, numBins - 1));
        binEnd   = qMax(binStart + 1, qMin(binEnd, numBins));

        // 频带内所有 bin 取均值（减少单 bin 噪声跳动）
        float sum = 0.0f;
        const int count = binEnd - binStart;
        for (int j = binStart; j < binEnd; ++j) {
            sum += magnitude[j];
        }
        bars[i] = sum / static_cast<float>(count);
    }

    // ── 3. 幂律压缩（可调强度）──
    //     pow(x, 0.10)=极平, pow(x, 0.33)=温和, pow(x, 0.66)=偏陡, pow(x, 1.0)=不压缩
    const float power = qBound(0.05f, compressionPower, 1.0f);

    for (int i = 0; i < barCount; ++i) {
        bars[i] = std::pow(qBound(0.0f, bars[i], 1.0f), power);
    }

    return bars;
}

} // namespace SpectrumProcessor
