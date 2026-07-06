#include "ResultWidget.h"
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QRadialGradient>
#include <QFont>
#include <QFrame>
#include <QIcon>

ResultWidget::ResultWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(8);
    layout->setContentsMargins(60, 30, 60, 30);

    // ── 生存模式标签 ──
    m_survivalLabel = new QLabel(this);
    QFont survivalFont = m_survivalLabel->font();
    survivalFont.setPixelSize(22);
    survivalFont.setBold(true);
    m_survivalLabel->setFont(survivalFont);
    m_survivalLabel->setAlignment(Qt::AlignCenter);
    m_survivalLabel->setStyleSheet("background: transparent;");
    m_survivalLabel->hide();
    layout->addWidget(m_survivalLabel);

    // ── 评级 ──
    m_gradeLabel = new QLabel(QStringLiteral("S"), this);
    QFont gradeFont = m_gradeLabel->font();
    gradeFont.setPixelSize(140);
    gradeFont.setBold(true);
    m_gradeLabel->setFont(gradeFont);
    m_gradeLabel->setAlignment(Qt::AlignCenter);
    m_gradeLabel->setStyleSheet("color: #00ff88; background: transparent;");
    layout->addWidget(m_gradeLabel);

    // ── 分数 ──
    m_scoreLabel = new QLabel(QStringLiteral("得分: 0"), this);
    QFont scoreFont = m_scoreLabel->font();
    scoreFont.setPixelSize(32);
    scoreFont.setBold(true);
    m_scoreLabel->setFont(scoreFont);
    m_scoreLabel->setAlignment(Qt::AlignCenter);
    m_scoreLabel->setStyleSheet("color: #ffffff; background: transparent;");
    layout->addWidget(m_scoreLabel);

    // ── 准确率 ──
    m_accuracyLabel = new QLabel(QStringLiteral("准确率: 0.00%"), this);
    QFont accFont = m_accuracyLabel->font();
    accFont.setPixelSize(20);
    m_accuracyLabel->setFont(accFont);
    m_accuracyLabel->setAlignment(Qt::AlignCenter);
    m_accuracyLabel->setStyleSheet("color: #8888aa; background: transparent;");
    layout->addWidget(m_accuracyLabel);

    layout->addSpacing(16);

    // ── 判定统计卡片 ──
    QWidget* statsPanel = new QWidget(this);
    statsPanel->setStyleSheet(
        "QWidget { background-color: rgba(22, 33, 62, 160); border: 1px solid #0f3460; border-radius: 12px; }");
    statsPanel->setFixedHeight(90);
    QHBoxLayout* statsLayout = new QHBoxLayout(statsPanel);
    statsLayout->setSpacing(0);
    statsLayout->setContentsMargins(20, 10, 20, 10);

    // 辅助函数：创建单个判定列
    auto makeStat = [&](const QString& title, const QString& color, QLabel*& label) -> QVBoxLayout* {
        QVBoxLayout* col = new QVBoxLayout();
        col->setSpacing(4);
        QLabel* t = new QLabel(title, statsPanel);
        t->setAlignment(Qt::AlignCenter);
        t->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; background: transparent; border: none;").arg(color));
        label = new QLabel(QStringLiteral("0"), statsPanel);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(QStringLiteral("color: %1; font-size: 26px; font-weight: bold; background: transparent; border: none;").arg(color));
        col->addWidget(t);
        col->addWidget(label);
        return col;
    };

    statsLayout->addLayout(makeStat("Perfect", "#00ff88", m_perfectLabel));

    // 分隔线
    QFrame* sep1 = new QFrame(statsPanel);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color: #1f1f3a; border: none;");
    sep1->setFixedWidth(1);
    statsLayout->addWidget(sep1);

    statsLayout->addLayout(makeStat("Good", "#ffff66", m_goodLabel));

    QFrame* sep2 = new QFrame(statsPanel);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setStyleSheet("color: #1f1f3a; border: none;");
    sep2->setFixedWidth(1);
    statsLayout->addWidget(sep2);

    statsLayout->addLayout(makeStat("Miss", "#e94560", m_missLabel));

    layout->addWidget(statsPanel);

    // ── 最大连击 ──
    m_maxComboLabel = new QLabel(QStringLiteral("最大连击: 0"), this);
    m_maxComboLabel->setAlignment(Qt::AlignCenter);
    QFont comboFont = m_maxComboLabel->font();
    comboFont.setPixelSize(18);
    m_maxComboLabel->setFont(comboFont);
    m_maxComboLabel->setStyleSheet("color: #bd93f9; background: transparent;");
    layout->addWidget(m_maxComboLabel);

    layout->addSpacing(12);

    // ── 本次排名提示 ──
    m_rankLabel = new QLabel(QStringLiteral("完成一曲，记入排行榜？"), this);
    m_rankLabel->setAlignment(Qt::AlignCenter);
    m_rankLabel->setStyleSheet("color: #8888aa; font-size: 15px; background: transparent;");
    layout->addWidget(m_rankLabel);

    layout->addSpacing(6);

    // ── 玩家名 + 记入按钮 ──
    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->setAlignment(Qt::AlignCenter);
    nameLayout->setSpacing(10);

    QLabel* nameLabel = new QLabel(QStringLiteral("玩家名"), this);
    nameLabel->setStyleSheet("color: #bd93f9; font-size: 15px; background: transparent;");
    nameLayout->addWidget(nameLabel);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setText(QStringLiteral("Player"));
    m_nameEdit->setMaxLength(20);
    m_nameEdit->setMinimumWidth(200);
    m_nameEdit->setFixedHeight(36);
    m_nameEdit->setStyleSheet(
        "QLineEdit { color: #ffffff; background-color: #16213e; "
        "border: 2px solid #0f3460; border-radius: 8px; padding: 4px 10px; font-size: 15px; }"
        "QLineEdit:focus { border-color: #00ff88; }");
    nameLayout->addWidget(m_nameEdit);

    m_submitBtn = new QPushButton(QStringLiteral("记入排行榜"), this);
    m_submitBtn->setMinimumSize(130, 36);
    m_submitBtn->setObjectName("actionButton");
    m_submitBtn->setIcon(QIcon(":/icons/trophy.svg"));
    m_submitBtn->setIconSize(QSize(20, 20));
    nameLayout->addWidget(m_submitBtn);

    layout->addLayout(nameLayout);

    layout->addSpacing(20);

    // ── 按钮行 ──
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(16);
    btnLayout->addStretch();

    m_retryBtn = new QPushButton(QStringLiteral("重试"), this);
    m_retryBtn->setMinimumSize(140, 42);
    m_retryBtn->setObjectName("actionButton");
    m_retryBtn->setIcon(QIcon(":/icons/replay.svg"));
    m_retryBtn->setIconSize(QSize(20, 20));
    btnLayout->addWidget(m_retryBtn);

    m_backBtn = new QPushButton(QStringLiteral("返回主菜单"), this);
    m_backBtn->setMinimumSize(140, 42);
    m_backBtn->setObjectName("backButton");
    m_backBtn->setIcon(QIcon(":/icons/arrow-left.svg"));
    m_backBtn->setIconSize(QSize(20, 20));
    btnLayout->addWidget(m_backBtn);

    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    layout->addStretch();

    // ── 信号连接 ──
    connect(m_retryBtn, &QPushButton::clicked, this, &ResultWidget::retryRequested);
    connect(m_backBtn, &QPushButton::clicked, this, &ResultWidget::backRequested);

    connect(m_submitBtn, &QPushButton::clicked, this, [this]() {
        QString name = m_nameEdit->text().trimmed();
        if (name.isEmpty()) {
            name = QStringLiteral("Player");
            m_nameEdit->setText(name);
        }
        m_submitBtn->setEnabled(false);
        m_submitBtn->setText(QStringLiteral("已记入"));
        emit scoreSubmitted(name);
    });
}

void ResultWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 深色渐变背景
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor(10, 10, 25));
    bg.setColorAt(0.5, QColor(13, 13, 31));
    bg.setColorAt(1.0, QColor(8, 8, 20));
    p.fillRect(rect(), bg);

    // 评级辉光：在 gradeLabel 中心位置绘制径向渐变
    if (m_gradeLabel && m_gradeLabel->isVisible()) {
        QPoint center = m_gradeLabel->geometry().center();
        QColor glowColor(m_gradeColor);
        glowColor.setAlpha(40);

        QRadialGradient glow(center, 220, center);
        glow.setColorAt(0.0, glowColor);
        glow.setColorAt(0.5, QColor(glowColor.red(), glowColor.green(), glowColor.blue(), 15));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setBrush(glow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(center, 220, 220);
    }
}

void ResultWidget::setResult(int score, int perfect, int good, int miss, int maxCombo, int totalNotes)
{
    // 默认隐藏生存模式标签（setSurvivalResult 会重新显示）
    m_survivalLabel->hide();

    int maxScore = totalNotes * 300;
    float ratio = (totalNotes > 0) ? static_cast<float>(score) / maxScore : 0.0f;
    float accuracy = ratio * 100.0f;

    m_scoreLabel->setText(QStringLiteral("得分: %1").arg(score));
    m_perfectLabel->setText(QString::number(perfect));
    m_goodLabel->setText(QString::number(good));
    m_missLabel->setText(QString::number(miss));
    m_maxComboLabel->setText(QStringLiteral("最大连击: %1").arg(maxCombo));

    // 准确率
    if (m_accuracyLabel)
        m_accuracyLabel->setText(QStringLiteral("准确率: %1%").arg(accuracy, 0, 'f', 2));

    QString grade = calculateGrade(score, perfect, good, miss, totalNotes);
    m_gradeLabel->setText(grade);

    // 评级颜色
    QString gradeColor;
    if (grade == QString::fromUtf8("\xcf\x86")) {
        gradeColor = "#ffd700";     // 金色（最高评级）
    } else if (grade == "SSS") {
        gradeColor = "#ff6b9d";     // 玫红
    } else if (grade == "SS") {
        gradeColor = "#ff9f43";     // 橙色
    } else if (grade == "S") {
        gradeColor = "#00ff88";     // 绿色
    } else if (grade == "A") {
        gradeColor = "#bd93f9";     // 紫色
    } else if (grade == "B") {
        gradeColor = "#8be9fd";     // 青色
    } else if (grade == "C") {
        gradeColor = "#ffff66";     // 黄色
    } else {
        gradeColor = "#e94560";     // 红色
    }
    m_gradeLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(gradeColor));

    // 缓存颜色供 paintEvent 辉光使用
    m_gradeColor = gradeColor;
    update();

    // 重置记入排行榜状态，供本局重新提交
    m_submitBtn->setEnabled(true);
    m_submitBtn->setText(QStringLiteral("记入排行榜"));
    m_rankLabel->setText(QStringLiteral("完成一曲，记入排行榜？"));
    m_rankLabel->setStyleSheet("color: #8888aa; font-size: 16px; background: transparent;");
}

void ResultWidget::setSurvivalResult(bool survived)
{
    m_survivalLabel->show();
    if (survived) {
        m_survivalLabel->setText(QString::fromUtf8(
            "\xe7\x94\x9f\xe5\xad\x98\xe6\xa8\xa1\xe5\xbc\x8f \xe2\x80\x94 \xe9\x80\x9a\xe5\x85\xb3"));
        m_survivalLabel->setStyleSheet(
            "color: #00ff88; font-size: 22px; font-weight: bold; background: transparent;");
    } else {
        m_survivalLabel->setText(QString::fromUtf8(
            "\xe7\x94\x9f\xe5\xad\x98\xe6\xa8\xa1\xe5\xbc\x8f \xe2\x80\x94 \xe5\xa4\xb1\xe8\xb4\xa5"));
        m_survivalLabel->setStyleSheet(
            "color: #e94560; font-size: 22px; font-weight: bold; background: transparent;");
    }
}

QString ResultWidget::calculateGrade(int score, int perfect, int good, int miss, int totalNotes) const
{
    if (totalNotes <= 0) return QStringLiteral("D");

    int maxScore = totalNotes * 300;
    float ratio = static_cast<float>(score) / static_cast<float>(maxScore);

    // φ 需要全 Perfect（AP）
    if (score == maxScore && miss == 0 && good == 0)
        return QString::fromUtf8("\xcf\x86");  // φ

    if (ratio >= 0.96f) return QStringLiteral("SSS");
    if (ratio >= 0.88f) return QStringLiteral("SS");
    if (ratio >= 0.78f) return QStringLiteral("S");
    if (ratio >= 0.65f) return QStringLiteral("A");
    if (ratio >= 0.50f) return QStringLiteral("B");
    if (ratio >= 0.25f) return QStringLiteral("C");
    return QStringLiteral("D");
}

void ResultWidget::presetName(const QString& name)
{
    m_nameEdit->setText(name.isEmpty() ? QStringLiteral("Player") : name);
}

void ResultWidget::showRank(int rank, int totalInBoard)
{
    if (rank <= 0) {
        m_rankLabel->setText(QStringLiteral("未进入前 50 名，再接再厉！"));
        m_rankLabel->setStyleSheet("color: #e94560; font-size: 16px; background: transparent;");
    } else {
        m_rankLabel->setText(QStringLiteral("本次排名  #%1 / %2").arg(rank).arg(totalInBoard));
        m_rankLabel->setStyleSheet("color: #00ff88; font-size: 18px; font-weight: bold; background: transparent;");
    }
}
