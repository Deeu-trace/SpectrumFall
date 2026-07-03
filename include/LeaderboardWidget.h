#pragma once

#include <QWidget>
#include <QVector>
#include <QMap>
#include "LeaderboardManager.h"

class QPushButton;
class QLabel;
class QVBoxLayout;

/// 排行榜浏览页面：卡片流式布局，每首歌一张卡片，展示 4K/6K top3 + 全屏查看
class LeaderboardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LeaderboardWidget(LeaderboardManager* lb, QWidget* parent = nullptr);

    /// 刷新整页。可选传入 songFileSize/laneCount 定位到某首歌某键数。
    void refresh(qint64 songFileSize = -1, int laneCount = 0);

signals:
    void backRequested();

private:
    LeaderboardManager* m_lb;

    QVBoxLayout* m_cardsLayout;   ///< 卡片容器布局
    QLabel* m_hintLabel;           ///< 空状态提示
    QPushButton* m_filterAllBtn;   ///< 全部 筛选按钮
    QPushButton* m_filter4KBtn;    ///< 4K 筛选按钮
    QPushButton* m_filter6KBtn;    ///< 6K 筛选按钮
    int m_currentFilter;           ///< 当前筛选: 0=全部, 4=4K, 6=6K

    QMap<qint64, QVector<LeaderboardEntry>> m_groupedEntries;

    void setupUI();
    void updateFilterButtons();
    void loadEntries();
    void rebuildCards();
    void clearCards();
    QWidget* buildCard(const QString& songName, const QVector<LeaderboardEntry>& entries);
    QWidget* buildTop3(const QVector<LeaderboardEntry>& modeEntries, int maxScore, int mode);
    void showFullDialog(const QString& songName, qint64 fileSize, int mode);
};
