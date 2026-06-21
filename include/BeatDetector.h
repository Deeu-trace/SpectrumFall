#pragma once

#include <QObject>
#include <QVector>

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
    /// 进度通过 progressChanged 信号报告，取消通过 requestCancel() 请求
    void analyze(const QVector<float>& pcm, int sampleRate);

    /// 请求取消正在进行的分析（跨线程安全）
    void requestCancel();

    /// 查询是否已请求取消（跨线程安全，供后台 lambda 检查）
    bool isCancelled() const { return m_cancelled; }

    /// 获取检测到的 BPM
    float bpm() const;

    /// 获取所有节拍点（含轨道分配）
    const QVector<BeatPoint>& beatPoints() const;

signals:
    /// 分析进度变化（0-100），跨线程安全发射
    void progressChanged(int percent);

private:
    float m_bpm;                    ///< 检测到的 BPM
    QVector<BeatPoint> m_beatPoints;///< 节拍点列表
    int m_fftSize;                  ///< FFT 窗口大小
    int m_hopSize;                  ///< 帧移大小
    volatile bool m_cancelled;      ///< 取消标志（volatile 保证跨线程可见性）

    // ── 轨道分配状态（对数频带 + 迟滞机制） ──
    static constexpr int NUM_LANES = 4;

    // ── 双押控制规则（使用整数百分比，避免浮点 constexpr 编译器兼容问题）──
    static constexpr int MIN_BEAT_BETWEEN_DOUBLE = 3;    ///< 连续双押最小间隔：至少隔 3 个节拍
    static constexpr int DOUBLE_PRESS_ENERGY_RATIO_PCT = 60; ///< 第 2 轨能量需达到第 1 轨的 60% 才允许双押
    static constexpr int WEAK_TONE_RATIO_PCT = 150;       ///< 第 1 轨能量 > 第 2 轨的 150% 时强制单键（弱音保护）

    /// 对数频带定义：{起始Hz, 截止Hz, 阈值补偿系数(除法)}
    struct BandDef {
        float freqLow;
        float freqHigh;
        float compensateDivisor;  ///< 能量除以此系数 → 低频轨降权、高频轨升权
    };
    static const BandDef s_bands[NUM_LANES];

    /// 迟滞状态：每个轨道的衰减惩罚计数器
    int m_hysteresisCount[NUM_LANES];

    /// 上一次产生双押的节拍下标（-1 = 无记录）
    int m_lastDoubleBeatIndex;

    /// 基于频谱能量分配轨道（对数频带 + 迟滞 + 双押控制）
    /// @param beatIndex 当前节拍在列表中的序号（用于双押间隔判定）
    /// @return 本拍应生成的轨道索引列表（1~2个）
    QVector<int> assignLanes(const QVector<float>& magnitude, int fftSize, int sampleRate, int beatIndex);

    /// 计算指定频带范围内的能量总和
    static float bandEnergy(const QVector<float>& magnitude, int binStart, int binEnd);

    /// typedef 避免 >> 被 MOC 误解为右移运算符
    typedef QVector<QVector<float> > MagnitudeCache;

    /// 计算 Spectral Flux（频谱通量），同时缓存每帧幅度谱
    QVector<float> computeSpectralFlux(const QVector<float>& pcm, int sampleRate,
                                       MagnitudeCache& magnitudeCache);

    /// 自适应阈值：局部均值 + δ * 局部标准差
    float adaptiveThreshold(const QVector<float>& flux, int index, int windowSize);

    /// 在 Spectral Flux 中寻找峰值
    QVector<int> findPeaks(const QVector<float>& flux, float threshold);

    /// 根据峰值间隔估计 BPM
    float estimateBPM(const QVector<int>& peakFrames, int hopSize, int sampleRate);
};
