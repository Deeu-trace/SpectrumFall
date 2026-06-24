#include "LeaderboardWidget.h"
#include "LeaderboardManager.h"

#include <QComboBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSet>

LeaderboardWidget::LeaderboardWidget(LeaderboardManager* lb, QWidget* parent)
    : QWidget(parent)
    , m_lb(lb)
{
    setupUI();
}

void LeaderboardWidget::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 30, 40, 30);
    layout->setSpacing(16);

    // 标题
    QLabel* titleLabel = new QLabel(QStringLiteral("排行榜"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPixelSize(36);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #00ff88; background: transparent;");
    layout->addWidget(titleLabel);

    // ── 筛选行 ──
    QHBoxLayout* filterRow = new QHBoxLayout();
    filterRow->setSpacing(12);

    QLabel* songLabel = new QLabel(QStringLiteral("歌曲"), this);
    songLabel->setStyleSheet("color: #bd93f9; font-size: 15px; background: transparent;");
    filterRow->addWidget(songLabel);

    m_songCombo = new QComboBox(this);
    m_songCombo->setMinimumWidth(300);
    m_songCombo->setStyleSheet(
        "QComboBox { color: #ffffff; background-color: #16213e; "
        "border: 2px solid #0f3460; border-radius: 8px; padding: 6px 10px; font-size: 15px; }"
        "QComboBox:hover { border-color: #00ff88; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { color: #ffffff; background-color: #16213e; "
        "selection-background-color: #0f3460; }");
    filterRow->addWidget(m_songCombo);

    filterRow->addSpacing(20);

    QLabel* modeLabel = new QLabel(QStringLiteral("键数"), this);
    modeLabel->setStyleSheet("color: #8be9fd; font-size: 15px; background: transparent;");
    filterRow->addWidget(modeLabel);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QStringLiteral("4 键"), 4);
    m_modeCombo->addItem(QStringLiteral("6 键"), 6);
    m_modeCombo->setCurrentIndex(1); // 默认 6 键
    m_modeCombo->setStyleSheet(m_songCombo->styleSheet());
    filterRow->addWidget(m_modeCombo);

    filterRow->addStretch();
    layout->addLayout(filterRow);

    // ── 表格 ──
    m_table = new QTableWidget(this);
    m_table->setColumnCount(9);
    m_table->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("排名") << QStringLiteral("玩家")
                      << QStringLiteral("分数") << QStringLiteral("评级")
                      << QStringLiteral("Perfect") << QStringLiteral("Good")
                      << QStringLiteral("Miss") << QStringLiteral("最大连击")
                      << QStringLiteral("日期"));
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_table->setStyleSheet(
        "QTableWidget { background-color: #0d0d1f; alternate-background-color: #12122a; "
        "color: #e0e0e0; gridline-color: #1f1f3a; border: 1px solid #0f3460; border-radius: 8px; "
        "font-size: 15px; }"
        "QHeaderView::section { background-color: #16213e; color: #00ff88; "
        "font-weight: bold; padding: 6px; border: none; border-bottom: 1px solid #0f3460; }"
        "QTableWidget::item { padding: 4px 8px; }"
        "QTableWidget::item:selected { background-color: #0f3460; color: #ffffff; }");
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 60);   // 排名
    m_table->setColumnWidth(1, 150);  // 玩家
    m_table->setColumnWidth(2, 90);   // 分数
    m_table->setColumnWidth(3, 60);   // 评级
    m_table->setColumnWidth(4, 70);   // Perfect
    m_table->setColumnWidth(5, 70);   // Good
    m_table->setColumnWidth(6, 70);   // Miss
    m_table->setColumnWidth(7, 90);   // 最大连击
    m_table->setColumnWidth(8, 140);  // 日期（最后一列，stretchLastSection 会拉伸填满）
    layout->addWidget(m_table, 1);

    // ── 空状态提示 ──
    m_hintLabel = new QLabel(this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet("color: #8888aa; font-size: 16px; background: transparent;");
    m_hintLabel->hide();
    layout->addWidget(m_hintLabel);

    // ── 返回按钮 ──
    m_backBtn = new QPushButton(QStringLiteral("返回主菜单"), this);
    m_backBtn->setMinimumSize(160, 42);
    m_backBtn->setObjectName("backButton");
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(m_backBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    // 连接
    connect(m_songCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LeaderboardWidget::onFilterChanged);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LeaderboardWidget::onFilterChanged);
    connect(m_backBtn, &QPushButton::clicked, this, &LeaderboardWidget::backRequested);
}

void LeaderboardWidget::loadSongList()
{
    m_songCombo->blockSignals(true);
    m_songCombo->clear();

    if (!m_lb) {
        m_songCombo->blockSignals(false);
        return;
    }

    QSet<qint64> seen;
    QVector<LeaderboardEntry> entries = m_lb->allEntries();
    for (const LeaderboardEntry& e : entries) {
        if (seen.contains(e.songFileSize)) continue;
        seen.insert(e.songFileSize);
        // 显示文件名；若同名加大小提示
        QString display = e.songFileName;
        m_songCombo->addItem(display, QVariant(static_cast<qlonglong>(e.songFileSize)));
    }

    m_songCombo->blockSignals(false);
}

void LeaderboardWidget::refreshTable()
{
    if (!m_lb || m_songCombo->count() == 0) {
        m_table->setRowCount(0);
        m_hintLabel->setText(QStringLiteral("暂无任何排行榜记录，先去打完一局吧！"));
        m_hintLabel->show();
        return;
    }

    qint64 fileSize = m_songCombo->currentData().toLongLong();
    int lane = m_modeCombo->currentData().toInt();

    QVector<LeaderboardEntry> entries = m_lb->entriesForSong(fileSize, lane);

    m_table->setRowCount(entries.size());

    for (int i = 0; i < entries.size(); ++i) {
        const LeaderboardEntry& e = entries[i];
        int rank = i + 1;

        auto makeItem = [](const QString& text, bool bold = false) {
            QTableWidgetItem* item = new QTableWidgetItem(text);
            if (bold) {
                QFont f = item->font();
                f.setBold(true);
                item->setFont(f);
            }
            item->setTextAlignment(Qt::AlignCenter);
            return item;
        };

        m_table->setItem(i, 0, makeItem(QStringLiteral("#%1").arg(rank), true));
        m_table->setItem(i, 1, makeItem(e.playerName));
        m_table->setItem(i, 2, makeItem(QString::number(e.score), true));
        m_table->setItem(i, 3, makeItem(e.grade));
        m_table->setItem(i, 4, makeItem(QString::number(e.perfect)));
        m_table->setItem(i, 5, makeItem(QString::number(e.good)));
        m_table->setItem(i, 6, makeItem(QString::number(e.miss)));
        m_table->setItem(i, 7, makeItem(QString::number(e.maxCombo)));
        m_table->setItem(i, 8, makeItem(e.playedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"))));

        // 第一名高亮
        if (rank == 1) {
            for (int c = 0; c < 9; ++c) {
                QTableWidgetItem* it = m_table->item(i, c);
                if (it) {
                    it->setForeground(QBrush(QColor(0, 255, 136)));
                }
            }
        }
    }

    if (entries.isEmpty()) {
        m_hintLabel->setText(QStringLiteral("该歌曲此键数暂无记录，打完一局后会自动记录"));
        m_hintLabel->show();
    } else {
        m_hintLabel->hide();
    }
}

void LeaderboardWidget::onFilterChanged()
{
    refreshTable();
}

void LeaderboardWidget::refresh(qint64 songFileSize, int laneCount)
{
    loadSongList();

    // 定位到指定歌曲
    if (songFileSize >= 0) {
        m_songCombo->blockSignals(true);
        for (int i = 0; i < m_songCombo->count(); ++i) {
            if (m_songCombo->itemData(i).toLongLong() == songFileSize) {
                m_songCombo->setCurrentIndex(i);
                break;
            }
        }
        m_songCombo->blockSignals(false);
    }

    // 定位到指定键数
    if (laneCount == 4 || laneCount == 6) {
        m_modeCombo->blockSignals(true);
        for (int i = 0; i < m_modeCombo->count(); ++i) {
            if (m_modeCombo->itemData(i).toInt() == laneCount) {
                m_modeCombo->setCurrentIndex(i);
                break;
            }
        }
        m_modeCombo->blockSignals(false);
    }

    refreshTable();
}
