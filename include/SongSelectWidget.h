#pragma once

#include <QWidget>
#include <QString>
#include <QVector>

class QPushButton;
class QLabel;
class QProgressBar;
class QListWidget;
class QListWidgetItem;
class QMenu;
class SpinnerWidget;

struct CacheEntry;  // 前向声明，避免包含 CacheManager.h

/// 歌曲选择页面：本地文件选择 + 手动 BPM 分析 + 历史记录
class SongSelectWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SongSelectWidget(QWidget* parent = nullptr);

    /// 获取当前选中的歌曲路径
    QString selectedSong() const;

    /// 设置分析进度（0-100），同时管理分析状态 UI
    void setAnalysisProgress(int percent);

    /// 更新时长显示
    void setDurationDisplay(qint64 ms);

    /// 分析完成，更新 UI 为成功状态
    void onAnalysisComplete(float bpm);

    /// 显示分析错误信息，重新启用按钮
    void showAnalysisError(const QString& message);

    /// 重置到初始状态（无文件选中）
    void resetState();

    /// 刷新历史记录列表
    void refreshHistory(const QVector<CacheEntry>& entries);

    /// 从缓存加载完成，设置 UI 为"已分析"状态
    void loadFromCache(const QString& filePath, float bpm, qint64 durationMs);

signals:
    void analyzeRequested(const QString& path);
    void visualizeRequested();
    void gameRequested();
    void backRequested();
    void historySelected(const QString& filePath);  ///< 点击历史记录条目
    void historyDeleteRequested(const QString& filePath);  ///< 删除历史记录条目

private slots:
    void onSelectFileClicked();
    void onAnalyzeClicked();
    void onVisualizeClicked();
    void onGameClicked();
    void onHistoryItemClicked(QListWidgetItem* item);
    void onHistoryContextMenu(const QPoint& pos);
    void onHistoryDeleteClicked();

private:
    void enterAnalyzingState();
    void exitAnalyzingState();
    QString formatFileSize(qint64 bytes) const;
    QString formatDuration(qint64 ms) const;
    QString formatDateTime(const QDateTime& dt) const;

    QPushButton* m_selectFileBtn;    ///< 选择/重新选择文件按钮
    QPushButton* m_analyzeBtn;       ///< 开始分析 BPM 按钮
    QPushButton* m_visualizeBtn;     ///< 可视化模式按钮
    QPushButton* m_gameBtn;          ///< 开始游戏按钮
    QPushButton* m_backBtn;          ///< 返回按钮

    QLabel* m_fileNameLabel;         ///< 文件名
    QLabel* m_fileSizeLabel;         ///< 文件大小
    QLabel* m_durationLabel;         ///< 时长
    QLabel* m_bpmLabel;              ///< BPM 显示
    QLabel* m_errorLabel;            ///< 错误信息

    QProgressBar* m_progressBar;     ///< 分析进度条
    SpinnerWidget* m_spinner;        ///< 转圈加载动画

    QWidget* m_fileInfoPanel;        ///< 文件信息+分析区域容器

    // 历史记录
    QLabel* m_historyTitle;          ///< 历史记录标题
    QListWidget* m_historyList;      ///< 历史记录列表
    QMenu* m_historyMenu;            ///< 右键菜单

    QString m_selectedPath;          ///< 当前选中文件路径
    bool m_analysisDone;             ///< 分析是否已完成
};
