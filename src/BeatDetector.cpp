#include "BeatDetector.h"
#include "FFTAnalyzer.h"
#include <QMap>
#include <cmath>
#include <algorithm>

BeatDetector::BeatDetector(QObject* parent)
    : QObject(parent)
    , m_bpm(0.0f)
    , m_fftSize(1024)    // 节拍检测用 1024 即可，速度比 2048 快 ~2x
    , m_hopSize(1024)    // 帧移 = fftSize，无重叠，总帧数减少 ~2x
{
}

void BeatDetector::analyze(const QVector<float>& pcm, int sampleRate)
{
    m_beatPoints.clear();
    m_bpm = 0.0f;

    if (pcm.isEmpty() || sampleRate <= 0) {
        return;
    }

    // 1. 计算 Spectral Flux，同时缓存每帧幅度谱用于后续轨道分配
    QVector<QVector<float>> magnitudeCache;
    QVector<float> flux = computeSpectralFlux(pcm, sampleRate, magnitudeCache);

    if (flux.isEmpty()) {
        return;
    }

    // 2. 自适应阈值 + 峰值检测
    float delta = 0.5f;
    QVector<float> threshold(flux.size());
    int adaptiveWindow = 10;

    for (int i = 0; i < flux.size(); ++i) {
        threshold[i] = adaptiveThreshold(flux, i, adaptiveWindow) + delta * 0.5f;
    }

    QVector<int> peakFrames = findPeaks(flux, 0.0f);

    QVector<int> filteredPeaks;
    for (int idx : peakFrames) {
        if (idx < threshold.size() && flux[idx] > threshold[idx]) {
            filteredPeaks.append(idx);
        }
    }

    // 3. 估计 BPM
    if (!filteredPeaks.isEmpty()) {
        m_bpm = estimateBPM(filteredPeaks, m_hopSize, sampleRate);
    }

    // 4. 将峰值帧索引转换为毫秒时间戳，用缓存的幅度谱分配轨道
    for (int frameIdx : filteredPeaks) {
        qint64 timestampMs = static_cast<qint64>(frameIdx) * m_hopSize * 1000 / sampleRate;

        int lane = 0;
        if (frameIdx < magnitudeCache.size() && !magnitudeCache[frameIdx].isEmpty()) {
            lane = assignLane(magnitudeCache[frameIdx], m_fftSize, sampleRate);
        }

        BeatPoint bp;
        bp.timestampMs = timestampMs;
        bp.lane = lane;
        m_beatPoints.append(bp);
    }

    // 5. 过滤间隔太近的节拍（< 100ms）
    QVector<BeatPoint> filtered;
    for (const BeatPoint& bp : m_beatPoints) {
        if (filtered.isEmpty() || (bp.timestampMs - filtered.last().timestampMs) >= 100) {
            filtered.append(bp);
        }
    }
    m_beatPoints = filtered;
}

float BeatDetector::bpm() const
{
    return m_bpm;
}

const QVector<BeatPoint>& BeatDetector::beatPoints() const
{
    return m_beatPoints;
}

QVector<float> BeatDetector::computeSpectralFlux(const QVector<float>& pcm, int sampleRate,
                                                  QVector<QVector<float>>& magnitudeCache)
{
    FFTAnalyzer fft(m_fftSize);
    int totalFrames = (pcm.size() - m_fftSize) / m_hopSize + 1;
    if (totalFrames <= 1) {
        return QVector<float>();
    }

    QVector<float> flux(totalFrames, 0.0f);
    magnitudeCache.resize(totalFrames);

    QVector<float> prevMagnitude;
    int halfN = m_fftSize / 2;

    for (int frame = 0; frame < totalFrames; ++frame) {
        int startSample = frame * m_hopSize;

        // 提取当前帧
        QVector<float> frameData(m_fftSize, 0.0f);
        for (int i = 0; i < m_fftSize && (startSample + i) < pcm.size(); ++i) {
            frameData[i] = pcm[startSample + i];
        }

        // 计算频谱
        QVector<float> magnitude;
        fft.compute(frameData, magnitude);

        // 缓存幅度谱用于后续轨道分配
        magnitudeCache[frame] = magnitude;

        // 计算 Spectral Flux
        if (!prevMagnitude.isEmpty()) {
            float sf = 0.0f;
            for (int i = 0; i < halfN && i < magnitude.size() && i < prevMagnitude.size(); ++i) {
                float diff = magnitude[i] - prevMagnitude[i];
                if (diff > 0.0f) {
                    sf += diff;
                }
            }
            flux[frame] = sf;
        }

        prevMagnitude = std::move(magnitude);
    }

    return flux;
}

float BeatDetector::adaptiveThreshold(const QVector<float>& flux, int index, int windowSize)
{
    int start = qMax(0, index - windowSize);
    int end = qMin(flux.size() - 1, index + windowSize);

    float sum = 0.0f;
    float sumSq = 0.0f;
    int count = end - start + 1;

    for (int i = start; i <= end; ++i) {
        sum += flux[i];
        sumSq += flux[i] * flux[i];
    }

    float mean = sum / count;
    float variance = sumSq / count - mean * mean;
    float stddev = std::sqrt(qMax(0.0f, variance));

    return mean + 0.5f * stddev;
}

QVector<int> BeatDetector::findPeaks(const QVector<float>& flux, float threshold)
{
    QVector<int> peaks;
    if (flux.size() < 3) {
        return peaks;
    }

    for (int i = 1; i < flux.size() - 1; ++i) {
        if (flux[i] > threshold &&
            flux[i] > flux[i - 1] &&
            flux[i] >= flux[i + 1]) {
            peaks.append(i);
        }
    }

    if (peaks.size() <= 1) {
        return peaks;
    }

    QVector<int> merged;
    merged.append(peaks[0]);
    for (int i = 1; i < peaks.size(); ++i) {
        if (peaks[i] - merged.last() < 5) {
            if (flux[peaks[i]] > flux[merged.last()]) {
                merged.last() = peaks[i];
            }
        } else {
            merged.append(peaks[i]);
        }
    }

    return merged;
}

float BeatDetector::estimateBPM(const QVector<int>& peakFrames, int hopSize, int sampleRate)
{
    if (peakFrames.size() < 2) {
        return 120.0f;
    }

    QVector<qint64> intervals;
    for (int i = 1; i < peakFrames.size(); ++i) {
        qint64 intervalMs = static_cast<qint64>(peakFrames[i] - peakFrames[i - 1]) * hopSize * 1000 / sampleRate;
        if (intervalMs > 0) {
            intervals.append(intervalMs);
        }
    }

    if (intervals.isEmpty()) {
        return 120.0f;
    }

    QMap<int, int> histogram;
    for (qint64 interval : intervals) {
        int bin = static_cast<int>(interval / 10);
        histogram[bin]++;
    }

    int maxCount = 0;
    int maxBin = 0;
    for (auto it = histogram.begin(); it != histogram.end(); ++it) {
        if (it.value() > maxCount) {
            maxCount = it.value();
            maxBin = it.key();
        }
    }

    float medianIntervalMs = static_cast<float>(maxBin) * 10.0f + 5.0f;
    if (medianIntervalMs > 0) {
        float bpm = 60000.0f / medianIntervalMs;
        if (bpm < 60.0f) bpm *= 2.0f;
        if (bpm > 200.0f) bpm /= 2.0f;
        return bpm;
    }

    return 120.0f;
}

int BeatDetector::assignLane(const QVector<float>& magnitude, int fftSize, int sampleRate)
{
    if (magnitude.isEmpty()) {
        return 0;
    }

    int halfN = magnitude.size();
    int bandSize = halfN / 4;

    float bandEnergy[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    for (int band = 0; band < 4; ++band) {
        int start = band * bandSize;
        int end = qMin(start + bandSize, halfN);
        for (int i = start; i < end; ++i) {
            bandEnergy[band] += magnitude[i];
        }
    }

    int maxBand = 0;
    for (int i = 1; i < 4; ++i) {
        if (bandEnergy[i] > bandEnergy[maxBand]) {
            maxBand = i;
        }
    }

    return maxBand;
}
