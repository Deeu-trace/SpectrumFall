#include "BeatDetector.h"
#include "FFTAnalyzer.h"
#include <QMap>
#include <cmath>
#include <algorithm>

// ── 对数频带定义 ──────────────────────────────────────────
// 采样率 44100Hz，FFT 1024 → 每个 bin = 44100/1024 ≈ 43.07Hz
// D: 60-250Hz   → bin 1~5    (低频，鼓/贝斯)，补偿 ÷2.0 降权
// F: 250-1000Hz → bin 6~23   (中低频，吉他/人声)，补偿 ÷1.3
// J: 1000-4000Hz → bin 24~92  (中高频，合成器/镲)，补偿 ÷0.7 升权
// K: 4000-22050Hz → bin 93~511 (高频，气息/泛音)，补偿 ÷0.3 强升权
const BeatDetector::BandDef BeatDetector::s_bands[NUM_LANES] = {
    {  60.0f,   250.0f, 2.0f },   // D 轨
    { 250.0f,  1000.0f, 1.3f },   // F 轨
    {1000.0f,  4000.0f, 0.7f },   // J 轨
    {4000.0f, 22050.0f, 0.3f },   // K 轨
};

BeatDetector::BeatDetector(QObject* parent)
    : QObject(parent)
    , m_bpm(0.0f)
    , m_fftSize(1024)    // 节拍检测用 1024 即可，速度比 2048 快 ~2x
    , m_hopSize(1024)    // 帧移 = fftSize，无重叠，总帧数减少 ~2x
    , m_cancelled(false)
    , m_hysteresisCount{0, 0, 0, 0}
    , m_lastDoubleBeatIndex(-1)
{
}

void BeatDetector::requestCancel()
{
    m_cancelled = true;
}

void BeatDetector::analyze(const QVector<float>& pcm, int sampleRate)
{
    m_beatPoints.clear();
    m_bpm = 0.0f;
    m_cancelled = false;

    // 重置迟滞状态
    for (int i = 0; i < NUM_LANES; ++i) {
        m_hysteresisCount[i] = 0;
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
}

float BeatDetector::bpm() const
{
    return m_bpm;
}

const QVector<BeatPoint>& BeatDetector::beatPoints() const
{
    return m_beatPoints;
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

    float binHz = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

    // 1. 计算每个对数频带的原始能量
    float rawEnergy[NUM_LANES];
    for (int i = 0; i < NUM_LANES; ++i) {
        int binStart = static_cast<int>(s_bands[i].freqLow / binHz);
        int binEnd   = static_cast<int>(s_bands[i].freqHigh / binHz);
        // 确保至少有 1 个 bin
        binEnd = qMax(binEnd, binStart + 1);
        rawEnergy[i] = bandEnergy(magnitude, binStart, binEnd);
    }

    // 2. 补偿系数处理：低频轨降权、高频轨升权
    float compensated[NUM_LANES];
    for (int i = 0; i < NUM_LANES; ++i) {
        compensated[i] = rawEnergy[i] / s_bands[i].compensateDivisor;
    }

    // 3. 迟滞惩罚：刚触发过的轨道，能量乘以衰减系数
    //    m_hysteresisCount > 0 时，惩罚 = 0.5^(count)，每拍减 1
    float hysteresisFactor[NUM_LANES];
    for (int i = 0; i < NUM_LANES; ++i) {
        if (m_hysteresisCount[i] > 0) {
            // count=1 → 0.5, count=2 → 0.25
            hysteresisFactor[i] = std::pow(0.5f, static_cast<float>(m_hysteresisCount[i]));
        } else {
            hysteresisFactor[i] = 1.0f;
        }
    }

    float finalEnergy[NUM_LANES];
    for (int i = 0; i < NUM_LANES; ++i) {
        finalEnergy[i] = compensated[i] * hysteresisFactor[i];
    }

    // 4. 按最终能量排序（降序）
    int indices[NUM_LANES] = {0, 1, 2, 3};
    std::sort(indices, indices + NUM_LANES, [&finalEnergy](int a, int b) {
        return finalEnergy[a] > finalEnergy[b];
    });

    // 5. 取能量最高的轨道（单键，必须 > 0）
    int topLane = indices[0];
    if (finalEnergy[topLane] > 0.0f) {
        result.append(topLane);
    }

    // 安全兜底：如果没有任何轨道有能量，分配 D 轨
    if (result.isEmpty()) {
        result.append(0);
    }

    // 6. 双押判定（核心修改：不再固定取 Top2）
    //    三重条件全部满足才允许双押：
    //    a) 间隔约束：距上次双押至少隔 MIN_BEAT_BETWEEN_DOUBLE 个节拍
    //    b) 能量比值：第 2 轨能量 ≥ 第 1 轨 × DOUBLE_PRESS_ENERGY_RATIO_PCT%
    //    c) 弱音保护：第 1 轨能量 < 第 2 轨 × WEAK_TONE_RATIO_PCT%（防单轨独大）
    bool allowDouble = false;
    if (finalEnergy[indices[0]] > 0.0f && finalEnergy[indices[1]] > 0.0f) {
        // 条件 a: 间隔约束
        bool intervalOk = (m_lastDoubleBeatIndex < 0) ||
                          (beatIndex - m_lastDoubleBeatIndex >= MIN_BEAT_BETWEEN_DOUBLE);

        // 条件 b: 能量比值（第 2 轨不能太弱，用整数乘 100 避免浮点除法）
        bool energyRatioOk = finalEnergy[indices[1]] * 100 >=
                              finalEnergy[indices[0]] * DOUBLE_PRESS_ENERGY_RATIO_PCT;

        // 条件 c: 弱音保护（第 1 轨不能远超第 2 轨）
        bool notDominant = finalEnergy[indices[0]] * 100 <
                           finalEnergy[indices[1]] * WEAK_TONE_RATIO_PCT;

        allowDouble = intervalOk && energyRatioOk && notDominant;
    }

    if (allowDouble) {
        int secondLane = indices[1];
        // 确保不与 topLane 重复
        if (secondLane != topLane) {
            result.append(secondLane);
            m_lastDoubleBeatIndex = beatIndex;
        }
    }

    // 7. 更新迟滞状态
    //    本次触发的轨道：设置惩罚计数 = 2（接下来 2 拍内惩罚递减）
    //    未触发的轨道：计数 -1（最低到 0）
    bool triggered[NUM_LANES] = {false, false, false, false};
    for (int lane : result) {
        triggered[lane] = true;
    }
    for (int i = 0; i < NUM_LANES; ++i) {
        if (triggered[i]) {
            m_hysteresisCount[i] = 2;  // 接下来 2 拍受惩罚
        } else {
            m_hysteresisCount[i] = qMax(0, m_hysteresisCount[i] - 1);
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
