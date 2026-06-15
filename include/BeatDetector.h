#pragma once

#include <QObject>
#include <QVector>
#include <functional>

/// 节拍点数据结构
struct BeatPoint
{
    qint64 timestampMs;  ///< 节拍时间戳（毫秒）
    int lane;            ///< 分配轨道（0=D, 1=F, 2=J, 3=K）
};

/// 节拍检测器：基于 Spectral Flux + 自适应阈值
class BeatDetector : public QObject
{
    Q_OBJECT

public:
    explicit BeatDetector(QObject* parent = nullptr);

    /// 分析 PCM 数据，检测节拍
    /// @param progressCallback 进度回调（0-100），在后台线程调用
    /// @param cancelFlag 取消标志，置 true 时提前终止分析
    void analyze(const QVector<float>& pcm, int sampleRate,
                 std::function<void(int)> progressCallback = nullptr,
                 volatile bool* cancelFlag = nullptr);

    /// 获取检测到的 BPM
    float bpm() const;

    /// 获取所有节拍点（含轨道分配）
    const QVector<BeatPoint>& beatPoints() const;

private:
    float m_bpm;                    ///< 检测到的 BPM
    QVector<BeatPoint> m_beatPoints;///< 节拍点列表
    int m_fftSize;                  ///< FFT 窗口大小
    int m_hopSize;                  ///< 帧移大小

    // ── 轨道分配状态（对数频带 + 迟滞机制） ──
    static constexpr int NUM_LANES = 4;

    /// 对数频带定义：{起始Hz, 截止Hz, 阈值补偿系数(除法)}
    struct BandDef {
        float freqLow;
        float freqHigh;
        float compensateDivisor;  ///< 能量除以此系数 → 低频轨降权、高频轨升权
    };
    static const BandDef s_bands[NUM_LANES];

    /// 迟滞状态：每个轨道的衰减惩罚计数器
    int m_hysteresisCount[NUM_LANES];

    /// 基于频谱能量分配轨道（对数频带 + 迟滞 + 双押限制）
    /// @return 本拍应生成的轨道索引列表（1~2个）
    QVector<int> assignLanes(const QVector<float>& magnitude, int fftSize, int sampleRate);

    /// 计算指定频带范围内的能量总和
    static float bandEnergy(const QVector<float>& magnitude, int binStart, int binEnd);

    /// 计算 Spectral Flux（频谱通量），同时缓存每帧幅度谱
    QVector<float> computeSpectralFlux(const QVector<float>& pcm, int sampleRate,
                                       QVector<QVector<float>>& magnitudeCache,
                                       std::function<void(int)> progressCallback = nullptr,
                                       volatile bool* cancelFlag = nullptr);

    /// 自适应阈值：局部均值 + δ * 局部标准差
    float adaptiveThreshold(const QVector<float>& flux, int index, int windowSize);

    /// 在 Spectral Flux 中寻找峰值
    QVector<int> findPeaks(const QVector<float>& flux, float threshold);

    /// 根据峰值间隔估计 BPM
    float estimateBPM(const QVector<int>& peakFrames, int hopSize, int sampleRate);
};
