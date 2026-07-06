#pragma once

#include <QObject>
#include <QVector>

/// 音频特征（用于情绪分类）
struct AudioFeatures
{
    float bpm;           ///< 检测到的 BPM
    float lowFreqRatio;  ///< 低频能量占比 (0-1)
    float avgEnergy;     ///< 平均能量 (归一化 0-1)
};

/// 节拍点数据结构
struct BeatPoint
{
    qint64 timestampMs;  ///< 节拍时间戳（毫秒）
    int lane;            ///< 分配轨道（4键:0=D,1=F,2=J,3=K / 6键:0=S,1=D,2=F,3=J,4=K,5=L）
};

/// 节拍检测器：基于 Spectral Flux + 自适应阈值 + 动态归一化
class BeatDetector : public QObject
{
    Q_OBJECT

public:
    explicit BeatDetector(QObject* parent = nullptr);

    /// 分析 PCM 数据，检测节拍
    /// @param laneCount 轨道数（4 或 6）
    void analyze(const QVector<float>& pcm, int sampleRate, int laneCount);

    /// 请求取消正在进行的分析（跨线程安全）
    void requestCancel();

    /// 查询是否已请求取消（跨线程安全，供后台 lambda 检查）
    bool isCancelled() const { return m_cancelled; }

    /// 获取检测到的 BPM
    float bpm() const;

    /// 获取所有节拍点（含轨道分配）
    const QVector<BeatPoint>& beatPoints() const;

    /// 获取音频特征（用于情绪分类）
    AudioFeatures audioFeatures() const;

signals:
    void progressChanged(int percent);

private:
    float m_bpm;
    QVector<BeatPoint> m_beatPoints;
    int m_fftSize;
    int m_hopSize;
    volatile bool m_cancelled;
    int m_laneCount;          ///< 当前轨道数（4 或 6）

    // 音频特征（analyze 末尾计算）
    float m_lowFreqRatio = 0;
    float m_avgEnergy = 0;

    static constexpr int MAX_LANES = 6;

    // ── 双押控制规则 ──
    static constexpr int MIN_BEAT_BETWEEN_DOUBLE = 3;
    static constexpr int DOUBLE_PRESS_ENERGY_RATIO_PCT = 60;
    static constexpr int WEAK_TONE_RATIO_PCT = 150;

    struct BandDef {
        float freqLow;
        float freqHigh;
    };
    static const BandDef s_bands4[4];
    static const BandDef s_bands6[6];

    // ── 迟滞 + 动态归一化 + 配额 ──
    int m_hysteresisCount[MAX_LANES];
    float m_bandSum[MAX_LANES];     ///< 每轨历史能量总和（归一化分母）
    int m_bandCount[MAX_LANES];     ///< 每轨贡献次数
    int m_laneStarvation[MAX_LANES]; ///< 每轨自上次触发以来的节拍数（配额机制）

    int m_lastDoubleBeatIndex;

    QVector<int> assignLanes(const QVector<float>& magnitude, int fftSize, int sampleRate, int beatIndex);
    static float bandEnergy(const QVector<float>& magnitude, int binStart, int binEnd);

    typedef QVector<QVector<float> > MagnitudeCache;
    QVector<float> computeSpectralFlux(const QVector<float>& pcm, int sampleRate,
                                       MagnitudeCache& magnitudeCache);
    float adaptiveThreshold(const QVector<float>& flux, int index, int windowSize);
    QVector<int> findPeaks(const QVector<float>& flux, float threshold);
    float estimateBPM(const QVector<int>& peakFrames, int hopSize, int sampleRate);
};
