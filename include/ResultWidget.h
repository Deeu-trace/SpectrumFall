#pragma once

#include <QWidget>

class QPushButton;
class QLineEdit;
class QLabel;

/// 结算页面：得分、各判定数、最大连击、评级(S/A/B/C/D)、重试/返回按钮
class ResultWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ResultWidget(QWidget* parent = nullptr);

    /// 设置结算数据
    void setResult(int score, int perfect, int good, int miss, int maxCombo, int totalNotes);

    /// 预填玩家名（来自上次记录）
    void presetName(const QString& name);

    /// 显示本次在排行榜中的排名
    void showRank(int rank, int totalInBoard);

signals:
    void retryRequested();
    void backRequested();
    /// 玩家点击“记入排行榜”，携带输入的玩家名
    void scoreSubmitted(const QString& playerName);

private:
    QLabel* m_scoreLabel;       ///< 总分标签
    QLabel* m_gradeLabel;       ///< 评级标签
    QLabel* m_perfectLabel;     ///< Perfect 数量标签
    QLabel* m_goodLabel;        ///< Good 数量标签
    QLabel* m_missLabel;        ///< Miss 数量标签
    QLabel* m_maxComboLabel;    ///< 最大连击标签
    QLineEdit* m_nameEdit;      ///< 玩家名输入框
    QPushButton* m_submitBtn;   ///< 记入排行榜按钮
    QLabel* m_rankLabel;        ///< 本次排名标签
    QPushButton* m_retryBtn;    ///< 重试按钮
    QPushButton* m_backBtn;     ///< 返回按钮

    /// 计算评级
    QString calculateGrade(int score, int totalNotes) const;
};
