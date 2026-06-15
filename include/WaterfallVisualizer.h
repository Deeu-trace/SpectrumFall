#pragma once

#include "VisualizerBase.h"
#include <QList>
#include <QVector>

/// 瀑布图可视化：历史频谱从上到下滚动，颜色映射能量大小
class WaterfallVisualizer : public VisualizerBase
{
    Q_OBJECT

public:
    explicit WaterfallVisualizer(QWidget* parent = nullptr);

    void setSpectrumData(const QVector<float>& magnitude) override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QList<QVector<float>> m_history;  ///< 历史频谱数据
    int m_maxHistory;                 ///< 最大历史帧数

    /// 能量值到颜色的映射
    QColor energyToColor(float value) const;
};
