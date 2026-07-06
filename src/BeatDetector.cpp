#include "BeatDetector.h"
#include "FFTAnalyzer.h"
#include <QMap>
#include <cmath>
#include <algorithm>

// ── 频带定义 ──────────────────────────────────────────────
const BeatDetector::BandDef BeatDetector::s_bands4[4] = {
    {  60.0f,   250.0f },   // D 轨
    { 250.0f,  1000.0f },   // F 轨
    {1000.0f,  4000.0f },   // J 轨
    {4000.0f, 22050.0f },   // K 轨
};
const BeatDetector::BandDef BeatDetector::s_bands6[6] = {
    {  20.0f,   120.0f },   // S 轨
    { 120.0f,   400.0f },   // D 轨
    { 400.0f,  1200.0f },   // F 轨
    {1200.0f,  3500.0f },   // J 轨
    {3500.0f,  8000.0f },   // K 轨
    {8000.0f, 22050.0f },   // L 轨
};

BeatDetector::BeatDetector(QObject* parent)
    : QObject(parent)
    , m_bpm(0.0f)
    , m_fftSize(1024)
    , m_hopSize(1024)
    , m_cancelled(false)
    , m_laneCount(6)
    , m_hysteresisCount{0, 0, 0, 0, 0, 0}
    , m_bandSum{0, 0, 0, 0, 0, 0}
    , m_bandCount{0, 0, 0, 0, 0, 0}
    , m_laneStarvation{0, 0, 0, 0, 0, 0}
    , m_lastDoubleBeatIndex(-1)
{
}

void BeatDetector::requestCancel()
{
    m_cancelled = true;
}

void BeatDetector::analyze(const QVector<float>& pcm, int sampleRate, int laneCount)
{
    m_beatPoints.clear();
    m_bpm = 0.0f;
    m_cancelled = false;
    m_laneCount = qBound(4, laneCount, 6);

    // 重置迟滞 + 归一化 + 配额状态
    for (int i = 0; i < MAX_LANES; ++i) {
        m_hysteresisCount[i] = 0;
        m_bandSum[i] = 0.0f;
        m_bandCount[i] = 0;
        m_laneStarvation[i] = 0;
    }
    m_lastDoubleBeatIndex = -1;

    if (pcm.isEmpty() || sampleRate <= 0) {
        return;
    }

    // 1. 计算 Spectral Flux，同时缓存每帧幅度谱用于后续轨道分配
    MagnitudeCache magnitudeCache;
    QVector<float> flux = computeSpectralFlux(pcm, sampleRate, magnitudeCache);

    if (flux.isEmpty() || m_cancelled) {
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
    //    assignLanes 返回 1~2 个轨道，每个轨道生成一个 BeatPoint
    for (int beatIdx = 0; beatIdx < filteredPeaks.size(); ++beatIdx) {
        int frameIdx = filteredPeaks[beatIdx];
        qint64 timestampMs = static_cast<qint64>(frameIdx) * m_hopSize * 1000 / sampleRate;

        QVector<int> lanes;
        if (frameIdx < magnitudeCache.size() && !magnitudeCache[frameIdx].isEmpty()) {
            lanes = assignLanes(magnitudeCache[frameIdx], m_fftSize, sampleRate, beatIdx);
        } else {
            lanes.append(0);
        }

        for (int lane : lanes) {
            BeatPoint bp;
            bp.timestampMs = timestampMs;
            bp.lane = lane;
            m_beatPoints.append(bp);
        }
    }

    // 5. 过滤间隔太近的节拍（< 100ms），但不同轨道的同一时间戳允许共存
    QVector<BeatPoint> filtered;
    for (const BeatPoint& bp : m_beatPoints) {
        // 同一时间戳的不同轨道 → 直接允许
        bool sameTimeExists = false;
        for (const BeatPoint& existing : filtered) {
            if (existing.timestampMs == bp.timestampMs && existing.lane == bp.lane) {
                sameTimeExists = true;
                break;
            }
        }
        if (sameTimeExists) continue;

        // 不同时间戳 → 检查 100ms 间隔（同轨道）
        bool tooClose = false;
        for (const BeatPoint& existing : filtered) {
            if (existing.lane == bp.lane &&
                bp.timestampMs - existing.timestampMs < 100 &&
                bp.timestampMs - existing.timestampMs > 0) {
                tooClose = true;
                break;
            }
        }
        if (!tooClose) {
            filtered.append(bp);
        }
    }
    m_beatPoints = filtered;

    // 6. 计算音频特征（用于情绪分类）
    if (!magnitudeCache.isEmpty()) {
        double totalLowEnergy = 0;
        double totalAllEnergy = 0;
        int frameCount = 0;
        for (const auto& mag : magnitudeCache) {
            if (mag.isEmpty()) continue;
            int binCount = mag.size();
            int lowBinEnd = qMax(1, binCount / 5); // 前 20% 频段
            double lowE = 0, allE = 0;
            for (int b = 0; b < binCount; ++b) {
                double e = static_cast<double>(mag[b]);
                allE += e;
                if (b < lowBinEnd) lowE += e;
            }
            totalLowEnergy += lowE;
            totalAllEnergy += allE;
            ++frameCount;
        }
        m_lowFreqRatio = (totalAllEnergy > 0)
            ? static_cast<float>(totalLowEnergy / totalAllEnergy) : 0.0f;
        m_avgEnergy = (frameCount > 0)
            ? static_cast<float>(totalAllEnergy / frameCount / (m_fftSize / 2)) : 0.0f;
    }
}

float BeatDetector::bpm() const
{
    return m_bpm;
}

const QVector<BeatPoint>& BeatDetector::beatPoints() const
{
    return m_beatPoints;
}

AudioFeatures BeatDetector::audioFeatures() const
{
    AudioFeatures f;
    f.bpm = m_bpm;
    f.lowFreqRatio = m_lowFreqRatio;
    f.avgEnergy = m_avgEnergy;
    return f;
}

// ── 对数频带轨道分配 ────────────────────────────────────

float BeatDetector::bandEnergy(const QVector<float>& magnitude, int binStart, int binEnd)
{
    float energy = 0.0f;
    int end = qMin(binEnd, magnitude.size());
    for (int i = qMax(0, binStart); i < end; ++i) {
        energy += magnitude[i];
    }
    return energy;
}

QVector<int> BeatDetector::assignLanes(const QVector<float>& magnitude, int fftSize, int sampleRate, int beatIndex)
{
    QVector<int> result;
    if (magnitude.isEmpty()) {
        result.append(0);
        return result;
    }

    int N = m_laneCount;
    const BandDef* bands = (N == 4) ? s_bands4 : s_bands6;
    float binHz = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

    // 1. 计算每轨能量 + 动态归一化
    float score[MAX_LANES];
    for (int i = 0; i < N; ++i) {
        int binStart = static_cast<int>(bands[i].freqLow / binHz);
        int binEnd   = static_cast<int>(bands[i].freqHigh / binHz);
        binEnd = qMax(binEnd, binStart + 1);
        float raw = bandEnergy(magnitude, binStart, binEnd);

        // 动态归一化：当前能量 ÷ 该轨历史均值
        float avg = (m_bandCount[i] > 0) ? (m_bandSum[i] / m_bandCount[i]) : 1.0f;
        score[i] = raw / (avg + 0.0001f);  // +epsilon 防 /0

        // 更新历史（衰减平均，防溢出）
        m_bandSum[i] += raw;
        m_bandCount[i] += 1;

        // 迟滞惩罚
        if (m_hysteresisCount[i] > 0) {
            score[i] *= std::pow(0.5f, static_cast<float>(m_hysteresisCount[i]));
        }
    }
    // 补零未使用的轨道
    for (int i = N; i < MAX_LANES; ++i) score[i] = 0.0f;

    // 2. 排序
    int indices[MAX_LANES] = {0, 1, 2, 3, 4, 5};
    std::sort(indices, indices + N, [&score](int a, int b) {
        return score[a] > score[b];
    });

    // 3. 取最高分轨道
    int topLane = indices[0];
    if (score[topLane] > 0.0f) {
        result.append(topLane);
    }
    if (result.isEmpty()) {
        result.append(0);
    }

    // 3.5 配额机制：如果某轨连续 8 个节拍没有音符，强制分配给它
    //     这保证 K/L 等高音轨道即使能量很低也会定期出现音符
    static const int STARVATION_THRESHOLD = 8;
    int starvedLane = -1;
    int maxStarvation = 0;
    for (int i = 0; i < N; ++i) {
        if (i != topLane && m_laneStarvation[i] > STARVATION_THRESHOLD &&
            m_laneStarvation[i] > maxStarvation) {
            maxStarvation = m_laneStarvation[i];
            starvedLane = i;
        }
    }
    if (starvedLane >= 0) {
        result.append(starvedLane);
    }

    // 4. 双押判定（配额触发不算双押，不影响间隔）
    if (starvedLane < 0) {  // 没有配额触发时才检查双押
        bool allowDouble = false;
        if (N >= 2 && score[indices[0]] > 0.0f && score[indices[1]] > 0.0f) {
            bool intervalOk = (m_lastDoubleBeatIndex < 0) ||
                              (beatIndex - m_lastDoubleBeatIndex >= MIN_BEAT_BETWEEN_DOUBLE);
            bool energyRatioOk = score[indices[1]] * 100 >= score[indices[0]] * DOUBLE_PRESS_ENERGY_RATIO_PCT;
            bool notDominant = score[indices[0]] * 100 < score[indices[1]] * WEAK_TONE_RATIO_PCT;
            allowDouble = intervalOk && energyRatioOk && notDominant;
        }
        if (allowDouble && indices[1] != topLane) {
            result.append(indices[1]);
            m_lastDoubleBeatIndex = beatIndex;
        }
    }

    // 5. 更新迟滞 + 配额
    bool triggered[MAX_LANES] = {false};
    for (int lane : result) triggered[lane] = true;
    for (int i = 0; i < N; ++i) {
        if (triggered[i]) {
            m_hysteresisCount[i] = 2;
            m_laneStarvation[i] = 0;  // 重置饥饿计数
        } else {
            m_hysteresisCount[i] = qMax(0, m_hysteresisCount[i] - 1);
            m_laneStarvation[i]++;   // 增加饥饿计数
        }
    }

    return result;
}

QVector<float> BeatDetector::computeSpectralFlux(const QVector<float>& pcm, int sampleRate,
                                                  MagnitudeCache& magnitudeCache)
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
        // 检查取消标志
        if (m_cancelled) {
            flux.clear();
            magnitudeCache.clear();
            return flux;
        }

        // 每 50 帧报告一次进度
        if (frame % 50 == 0) {
            emit progressChanged(frame * 100 / totalFrames);
        }

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

    // 最终进度
    emit progressChanged(100);

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
        // 只保留音乐上合理的间隔：200ms(300BPM) ~ 2000ms(30BPM)
        if (intervalMs >= 200 && intervalMs <= 2000) {
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
        // 循环校正直到 BPM 落入合理范围
        while (bpm < 60.0f)  bpm *= 2.0f;
        while (bpm > 200.0f) bpm /= 2.0f;
        return bpm;
    }

    return 120.0f;
}
