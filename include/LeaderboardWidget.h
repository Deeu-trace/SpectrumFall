#pragma once

#include <QWidget>

class LeaderboardManager;
class QComboBox;
class QTableWidget;
class QPushButton;
class QLabel;

/// 排行榜浏览页面：按歌曲 + 键数(4/6) 筛选，表格展示前 50 名
class LeaderboardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LeaderboardWidget(LeaderboardManager* lb, QWidget* parent = nullptr);

    /// 刷新整页（重新载入歌曲列表与表格）。
    /// 可选传入 songFileSize/laneCount 以直接定位到某首歌某键数。
    void refresh(qint64 songFileSize = -1, int laneCount = 0);

signals:
    void backRequested();

private slots:
    void onFilterChanged();

private:
    LeaderboardManager* m_lb;       ///< 排行榜数据源
    QComboBox* m_songCombo;        ///< 歌曲筛选
    QComboBox* m_modeCombo;        ///< 4键/6键筛选
    QTableWidget* m_table;         ///< 排名表格
    QLabel* m_hintLabel;           ///< 空状态提示
    QPushButton* m_backBtn;        ///< 返回按钮

    /// 构建 UI
    void setupUI();

    /// 从排行榜数据载入歌曲下拉列表（去重）
    void loadSongList();

    /// 根据当前筛选刷新表格
    void refreshTable();
};
