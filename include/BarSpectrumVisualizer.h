#pragma once

#include "VisualizerBase.h"
#include <QVector>

/// 柱状频谱可视化：竖直柱状图，颜色渐变（绿→黄→红），柱顶有下落峰值指示
class BarSpectrumVisualizer : public VisualizerBase
{
    Q_OBJECT

public:
    explicit BarSpectrumVisualizer(QWidget* parent = nullptr);

    void setSpectrumData(const QVector<float>& magnitude) override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_barCount;                    ///< 柱子数量
    QVector<float> m_peakValues;       ///< 峰值指示器位置
    QVector<float> m_peakFallSpeed;    ///< 峰值下落速度
};
