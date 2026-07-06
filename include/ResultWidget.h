#pragma once

#include <QWidget>

class QPushButton;
class QLineEdit;
class QLabel;

/// 结算页面：得分、准确率、各判定数、最大连击、评级、重试/返回按钮
class ResultWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ResultWidget(QWidget* parent = nullptr);

    /// 设置结算数据
    void setResult(int score, int perfect, int good, int miss, int maxCombo, int totalNotes);

    /// 设置生存模式结果（true=通关，false=血尽失败）
    void setSurvivalResult(bool survived);

    /// 预填玩家名（来自上次记录）
    void presetName(const QString& name);

    /// 显示本次在排行榜中的排名
    void showRank(int rank, int totalInBoard);

signals:
    void retryRequested();
    void backRequested();
    /// 玩家点击"记入排行榜"，携带输入的玩家名
    void scoreSubmitted(const QString& playerName);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* m_survivalLabel;    ///< 生存模式状态标签（通关/失败）
    QLabel* m_gradeLabel;       ///< 评级标签
    QLabel* m_scoreLabel;       ///< 总分标签
    QLabel* m_accuracyLabel;    ///< 准确率标签
    QLabel* m_perfectLabel;     ///< Perfect 数量标签
    QLabel* m_goodLabel;        ///< Good 数量标签
    QLabel* m_missLabel;        ///< Miss 数量标签
    QLabel* m_maxComboLabel;    ///< 最大连击标签
    QLineEdit* m_nameEdit;      ///< 玩家名输入框
    QPushButton* m_submitBtn;   ///< 记入排行榜按钮
    QLabel* m_rankLabel;        ///< 本次排名标签
    QPushButton* m_retryBtn;    ///< 重试按钮
    QPushButton* m_backBtn;     ///< 返回按钮

    // 缓存当前评级颜色（paintEvent 绘制背景辉光用）
    QString m_gradeColor = "#00ff88";

    /// 计算评级
    QString calculateGrade(int score, int perfect, int good, int miss, int totalNotes) const;
};
